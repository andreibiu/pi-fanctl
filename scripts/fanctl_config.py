#!/usr/bin/python3

import sys

from enum import Enum, IntEnum
from typing import Final, Self
from dataclasses import dataclass
from re import sub, split, findall
from os import getenv

# Utility Functions #

def as_type(type: type, value: str) -> any:
    try:
        return type(value)
    except:
        return None
    
def handle_error(message: str) -> None:
    print('[ERROR]', message, file=sys.stderr)
    sys.exit(-1)

# Constants #

POINTS_NUM = 8
BOARD_INFO_SOURCE_PATH = '/sys/firmware/devicetree/base/compatible'
BOOT_CONFIG_PATH = '/boot/config.txt'
DRIVER_NAME = getenv('DRIVER_NAME')
DRIVER_CONFIG_PATH = getenv('DRIVER_CONFIG_PATH')
DRIVER_DTS_PATH = getenv('DRIVER_DTS_PATH')
DRIVER_DTS_TEMPLATE_PATH = f'{getenv("DRIVER_DTS_TEMPLATE_BASE_PATH")}/{DRIVER_NAME}_%%%%.dts.template'
DRIVER_DTOVERLAY_LOAD_SETTING = f'dtoverlay={DRIVER_NAME}'
PI_5_DTS_PATH = getenv('PI_5_DTS_PATH')

# Enums & Dataclasses (model) #

class PwmPolarity(Enum):
    DIRECT = 'dir'
    INVERTED = 'inv'

    def __str__(self) -> str:
        match self:
            case PwmPolarity.DIRECT: return '0'
            case PwmPolarity.INVERTED: return '1'

class Mode(Enum):
    GPIO_PINS = 'gpio'
    FAN_HEADER = 'fanh'

    @property
    def driver_dts_template_path(self) -> str:
        return DRIVER_DTS_TEMPLATE_PATH.replace('%%%%', self.value);

@dataclass
class Config:
    board_id: Final[int]
    soc_name: Final[str]
    mode: Mode
    pwm_channel: int
    pwm_period: int
    pwm_polarity: PwmPolarity
    time_delay: int
    temperatures: list[int]
    temperatures_hyst: list[int]
    fan_speeds: list[int]

    def __init__(self) -> Self:
        self.mode = Mode.GPIO_PINS
        self.pwm_period = 20000
        self.pwm_polarity = PwmPolarity.DIRECT
        self.time_delay = 1000
        self.temperatures = [None] * POINTS_NUM
        self.temperatures_hyst = [None] * POINTS_NUM
        self.fan_speeds = [None] * POINTS_NUM
        with open(BOARD_INFO_SOURCE_PATH) as board_info_source:
            board_info = board_info_source.readline().replace('\x00', '').split(',')
            self.board_id = int(board_info[1].split('-')[0])
            self.soc_name = board_info[2]
            self.pwm_channel = 2 if self.is_rpi_5 else 0
    
    @property
    def is_rpi_5(self) -> bool:
        return self.board_id == 5
    
# Functions (logic) #
    
def parse_config() -> Config:
    config = Config()
    with open(DRIVER_CONFIG_PATH) as config_file:
        for line in config_file:
            line = line.strip()
            if line.startswith('#'):  # comment line
                continue
            match list(filter(None, split('(\d+|=|,)|\s', line))):
                case [ 'MODE', '=', mode ] \
                  if mode := as_type(Mode, mode):
                    config.mode = mode
                    if mode == Mode.FAN_HEADER and not config.is_rpi_5:
                        handle_error(f'Fan header config only available for Raspberry Pi 5 boards')
                case [ 'DELAY', '=', time_delay, 'ms' ] \
                  if time_delay := as_type(int, time_delay):
                    config.time_delay = time_delay
                case [ 'PWM_FREQ', '=', pwm_frequency, 'Hz' ] \
                  if pwm_frequency := as_type(int, pwm_frequency):
                    config.pwm_period = max(1, 1000000000 // pwm_frequency)
                case [ 'PWM_POL', '=', pwm_polarity ] \
                  if pwm_polarity := as_type(PwmPolarity, pwm_polarity):
                    config.pwm_polarity = pwm_polarity
                case [ 'POINT_', point_idx, '=', temperature, 'C', ',', hyst_temperature, 'C', ',', fan_speed, '%' ] \
                  if ((point_idx := as_type(int, point_idx)) and (point_idx := point_idx - 1) in range(0, POINTS_NUM)) and \
                     ((temperature := as_type(int, temperature)) and temperature in range(-128, 128)) and \
                     ((hyst_temperature := as_type(int, hyst_temperature)) and hyst_temperature >= 0) and \
                     ((fan_speed := as_type(int, fan_speed)) and fan_speed in range(1, 101)):
                    config.temperatures[point_idx] = temperature
                    config.temperatures_hyst[point_idx] = temperature - hyst_temperature
                    config.fan_speeds[point_idx] = fan_speed
                    if point_idx > 0:
                        if not config.temperatures[point_idx - 1]:
                            handle_error(f'Discontinuous fan curve; point {point_idx - 1} missing')
                        if config.temperatures[point_idx] <= config.temperatures[point_idx - 1] or \
                           config.temperatures_hyst[point_idx] <= config.temperatures_hyst[point_idx - 1] or \
                           config.fan_speeds[point_idx] <= config.fan_speeds[point_idx - 1]:
                            handle_error(f'Invalid data for curve point {point_idx}')
                case _:
                    handle_error(f'Invalid line in config: {line}')

    for point_idx in range(0, POINTS_NUM):
        if not config.temperatures[point_idx]:
            if point_idx == 0:
                handle_error(f'No fan curve provided')
            config.temperatures[point_idx] = config.temperatures[point_idx - 1]
            config.temperatures_hyst[point_idx] = config.temperatures_hyst[point_idx - 1]
            config.fan_speeds[point_idx] = config.fan_speeds[point_idx - 1]
    return config

def should_skip_driver_dts_fragment(line: str, config: Config) -> bool:
    return (tags := findall('#PI\d', line)) and (int(tags[0][3:]) != config.board_id)

def generate_driver_dts_overlay(config: Config) -> None:
    with open(config.mode.driver_dts_template_path, 'r') as dts_template_file, \
         open(DRIVER_DTS_PATH, 'w') as dts_file:
        in_skip_mode = False
        reached_first_lbrace = False
        reached_last_lbrace = False
        open_lbraces_num = 0
        for line in dts_template_file:
            if '#' in line:
                if in_skip_mode := in_skip_mode or should_skip_driver_dts_fragment(line, config):
                    open_lbraces_num += line.count('{')
                    if reached_first_lbrace := reached_first_lbrace or open_lbraces_num:
                        open_lbraces_num -= line.count('}')
                        if open_lbraces_num == 0:
                            in_skip_mode = False
                            reached_first_lbrace = False
                            reached_last_lbrace = True
                continue
            if reached_last_lbrace:
                reached_last_lbrace = False
                if not line.strip():
                    continue

            for key in findall('\$[A-Z_]+', line):
                value = str(config_value := config.__dict__[key[1:].lower()])
                line = line.replace(key, value[1:-1].replace(',', '') if isinstance(config_value, list) else value)
            dts_file.write(line)

def add_boot_config_settings() -> None:
    with open(BOOT_CONFIG_PATH, 'r') as boot_config_file:
        for line in boot_config_file:
            if sub('\s', '', line) == DRIVER_DTOVERLAY_LOAD_SETTING:
                return
    with open(BOOT_CONFIG_PATH, 'a') as boot_config_file:
        boot_config_file.writelines((
            '\n',
            '[all]', '\n',
            DRIVER_DTOVERLAY_LOAD_SETTING, '\n'
        ))

def remove_boot_config_settings() -> None:
    lines = []
    with open(BOOT_CONFIG_PATH, 'r') as boot_config_file:
        for line in boot_config_file:
            if sub('\s', '', line) == DRIVER_DTOVERLAY_LOAD_SETTING:
                end_index = -(lines[-1].strip() == '[all]') + -(not lines[-2].strip())
                lines = lines[:end_index] if end_index else lines
                continue
            lines.append(line)
    with open(BOOT_CONFIG_PATH, 'w') as boot_config_file:
        boot_config_file.writelines(lines)

def update_board_dts(config: Config) -> None:
    if not config.is_rpi_5:
        return
    
    try:
        lines = []
        with open(PI_5_DTS_PATH, 'r') as board_dts_file:
            found_skip_tag = False
            in_skip_node = False
            for line in board_dts_file:
                stripped_line = sub('\s', '', line)
                if not found_skip_tag and 'cooling_fan' in stripped_line:
                    found_skip_tag = True
                if found_skip_tag:
                    if '{' in stripped_line:
                        in_skip_node = True
                    if in_skip_node and (stripped_line.startswith('compatible=') or
                                            stripped_line.startswith('pwms=')):
                        continue
                    if '}' in stripped_line:
                        found_skip_tag = False
                        in_skip_node = False
                lines.append(line)
        with open(PI_5_DTS_PATH, 'w') as board_dts_file:
            board_dts_file.writelines(lines)
    except FileNotFoundError:
        pass

# Entry point #

if __name__ == '__main__':
    config = None
    for arg in sys.argv[1:]:
        match arg:
            case 'boot_add':
                add_boot_config_settings()
            case 'boot_remove':
                remove_boot_config_settings()
            case 'driver_dts':
                generate_driver_dts_overlay(config := config if config else parse_config())
            case 'board_dts':
                update_board_dts(config := config if config else parse_config())
            case _ as command:
                handle_error(f'Unknown command to execute: {command}')
