#!/usr/bin/python3

import re
import sys
from enum import IntEnum
from functools import partial

MAX_POINTS_NUM = 8
CONTROL_PERIOD_CFG_KEY = 'PERIOD_MS'

def clamp(min_value, max_value, value):
    return max(min(value, max_value), min_value)

class Curve(IntEnum):
    TEMPERATURE = 0
    TEMPERATURE_HYST = 1
    FAN_SPEED = 2

    def get_define(self, curve):
        main_noun = 'TEMPERATURE' if self.name[0] == 'T' else 'SPEED'
        key = f'CURVE_{self.name.replace(main_noun, main_noun + "S")}'
        value = str([point[self] for point in curve])[1:-1]
        return f'-D{key}="{value}"'

if __name__ == "__main__":
    try:
        control_period = 0
        curve = [None] * MAX_POINTS_NUM
        with open(sys.argv[1], "r") as config_file:
            for line in config_file:
                parts = re.split('=|,', line.strip())
                if len(parts) == 0 or not parts[0] or parts[0].startswith('#'):
                    continue
                if len(parts) == 2 and parts[0] == 'P':
                    control_period = max(int(parts[1]), 0)
                elif len(parts) == 4 and (curve_point_idx := int(parts[0]) - 1) in range(MAX_POINTS_NUM):
                    raw_values = list(map(int, parts[1:]))
                    curve[curve_point_idx] = [
                        temperature := clamp(-128, 127, raw_values[Curve.TEMPERATURE]),
                        clamp(-128, 127, temperature - max(0, raw_values[Curve.TEMPERATURE_HYST])),
                        clamp(0, 100, raw_values[Curve.FAN_SPEED])
                    ]

                    if curve_point_idx > 0:
                        prev_curve_point, crt_curve_point = curve[curve_point_idx - 1 : curve_point_idx + 1];
                        if prev_curve_point is not None and \
                        (crt_curve_point[Curve.TEMPERATURE] <= prev_curve_point[Curve.TEMPERATURE] or \
                            crt_curve_point[Curve.TEMPERATURE_HYST] < prev_curve_point[Curve.TEMPERATURE]):
                            raise ValueError(f'Invalid value for temperature or hysteresis at point {curve_point_idx}')
                else:
                    raise ValueError(f'Invalid configuration entry: {line}')

        missing_points = []
        for point_idx in range(MAX_POINTS_NUM):
            if curve[point_idx] is None:
                if point_idx == 0:
                    raise ValueError(f'Missing point(s) 1 for fan curve')
                missing_points.append(point_idx)
                curve[point_idx] = curve[point_idx - 1]
            else:
                if missing_points:
                    raise ValueError(f'Missing point(s) {str([point + 1 for point in missing_points])[1:-1]} for fan curve')
                prev_set_point_idx = point_idx
    except Exception as ex:
        print(ex, file=sys.stderr)
        sys.exit(-1)

    print(
        *map(partial(Curve.get_define, curve=curve), Curve),
        f'-DCONTROL_PERIOD_MS={control_period}' if control_period > 0 else ""
    )
