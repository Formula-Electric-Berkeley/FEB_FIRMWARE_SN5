# FEB CAN Library SN4

## Commands

The following commands reference the [cantools](https://cantools.readthedocs.io) documentation.

Generate DBC file:
```
python3 generate.py
```

Visualize DBC file:
```
python3 -m cantools dump gen/FEB_CAN.dbc
```

Generate C files from DBC file:
```
sudo python3 -m cantools generate_c_source -o gen gen/FEB_CAN.dbc
```

## Files and Directories

* `generate.py`: A Python file that generates a DBC file, using `*_messages.py` Python files.
* `*_messages.py`: A Python file that generates CAN messages for a single board (e.g., `bms_messages.py`).
* `/gen`: A directory for generated code.

## Setup

Recommendations
* Setup a Conda environment for this project.
* Install packages (inside Conda environment) using `pip`.

Packages
* cantools
