"""Verify independent alternative native axis grip/stretch experiments."""
import sys
from pathlib import Path
from verify_native_exercise import main

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_native_axis_alt.py <native-axis-alt.json>")
    raise SystemExit(main(
        Path(sys.argv[1]),
        expected={
            "axis-grip-2": ("199F", "TDbAxisLabelSet", 2, [0, 200, 0]),
            "axis-stretch-1": ("199F", "TDbAxisLabelSet", 1, [0, 200, 0]),
        },
        contract="stage1.native-axis-alt.v1",
        modes={"axis-grip-2": "grip", "axis-stretch-1": "stretch"},
    ))
