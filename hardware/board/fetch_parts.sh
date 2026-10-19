#!/usr/bin/env bash
# Fetch footprints + symbols for every LCSC part into footprints/lcsc.*
set -euo pipefail
cd "$(dirname "$0")"
PARTS=(
  C2913206  # ESP32 (ESP32-S3-MINI-1-N8)
  C265419   # JST5 (SM05B-SRSS-G-TB): the house USB connector
  C7519     # ESD (USBLC6-2SC6)
  C51118    # LDO (AP2112K-3.3TRG1)
  C20801    # PTC (SMD1206P110TF, 1.1 A hold)
  C78423    # TVS (SMBJ5.0A)
  C318884   # SW (TS-1187A-B-A-B): BOOT
  C558584   # EXP (MCP23017T-E/SS)
  C132291   # FUSB (FUSB302BMPX)
  C49851    # INA (INA226AIDGSR)
  C20917    # NMOS (AO3400A)
  C15127    # PMOS (AO3401A)
  C8545     # SMALLN (2N7002)
  C2128     # DIODE (1N4148WS)
  C94599    # BUZZER (MLT-8530)
  C5362174  # FPC12 (05A20H12P, 0.5 mm 12-pin, dual contact)
  C597821   # USBC (CX90B1-24P, full 24-pin)
  C7501850  # USBA (HC-USB3.0-L168-WP)
  C91146    # MICROB (U-F-M5DD-Y-L)
  C464586   # RJ45 (Amphenol RJHSE5380, shielded, no magnetics)
  C41950    # RLOAD (20R 2512 1W)
  C2903476  # RSHUNT (100mR 2512 3W)
  C25905    # R5K1 (and the footprint + symbol every 0402 resistor shares: R1K C11702, R10R C25077, R100R C25076, R100K C25741)
  C25744    # R10K
  C25900    # R4K7
  C1525     # C100N
  C52923    # C1U
  C19702    # C10U
)
# easyeda.com's CloudFront returns 403 for easyeda2kicad's default User-Agent, so run it
# through a one-line wrapper that sends a neutral UA instead.
FAILED=()
for c in "${PARTS[@]}"; do
  # resumable: skip parts whose symbol is already in the library
  grep -q "\"$c\"" footprints/lcsc.kicad_sym 2>/dev/null && continue
  sleep 20  # be polite: easyeda.com rate-limits bursts with a 403
  .venv/bin/python -c '
import sys
from easyeda2kicad.easyeda.easyeda_api import EasyedaApi
_init = EasyedaApi.__init__
def init(self):
    _init(self)
    self.headers["User-Agent"] = "katooni-board-fetch"
EasyedaApi.__init__ = init
from easyeda2kicad.__main__ import main
sys.argv[0] = "easyeda2kicad"
sys.exit(main())
' --symbol --footprint --lcsc_id="$c" --output footprints/lcsc --overwrite || FAILED+=("$c")
done
if ((${#FAILED[@]})); then echo "FAILED: ${FAILED[*]}" >&2; exit 1; fi
