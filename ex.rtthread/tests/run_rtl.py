#!/usr/bin/env python3
"""Build/run the PicoRV32 CPU model against the actual C2 BIN, without QEMU."""
import argparse
import hashlib
from pathlib import Path
import subprocess

RTL_SHA256 = "0836050971b3c6cdd28ac3b1e5719a67fb645161912bef1e472e63995ceb0622"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rtl", type=Path, required=True, help="Pinned picorv32.v")
    parser.add_argument("--verilator", default="verilator")
    parser.add_argument("--probe", type=Path, help="Optional c2_irq_probe.bin")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    rtl = args.rtl.resolve()
    if hashlib.sha256(rtl.read_bytes()).hexdigest() != RTL_SHA256:
        parser.error("RTL SHA256 mismatch; use the revision in README.md")
    binary = project / "dist/c2_rtthread.bin"
    if not binary.is_file():
        parser.error("Build c2_rtthread before running this test")
    build = project / "build/rtl"
    build.mkdir(parents=True, exist_ok=True)
    cmd = [args.verilator, "--cc", "--exe", "--build", "-j", "4", "-Wno-fatal",
           "--top-module", "picorv32", "-GENABLE_IRQ=1", "-GENABLE_IRQ_QREGS=0",
           "-GPROGADDR_RESET=0", "-GPROGADDR_IRQ=0", "-GCOMPRESSED_ISA=1",
           "-GENABLE_MUL=1", "-GENABLE_DIV=1", "-GBARREL_SHIFTER=1",
           "--Mdir", str(build), str(rtl), str(project / "tests/picorv32_sim.cpp")]
    with (build / "build.log").open("w") as log:
        subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)
    for wait in (0, 8):
        result = subprocess.run([str(build / "Vpicorv32"), str(binary), str(wait)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=60)
        (build / f"run-wait{wait}.log").write_text(result.stdout)
        print(f"Bus wait cycles: {wait}\n{result.stdout}", flush=True)
        result.check_returncode()
    if args.probe:
        probe = str(args.probe.resolve())
        subprocess.run([str(build / "Vpicorv32"), probe, "0", "probe-irq"],
                       check=True, timeout=60)
        disabled = project / "build/rtl-noirq"
        disabled.mkdir(parents=True, exist_ok=True)
        cmd = [str(disabled) if item == str(build) else
               "-GENABLE_IRQ=0" if item == "-GENABLE_IRQ=1" else item for item in cmd]
        with (disabled / "build.log").open("w") as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(disabled / "Vpicorv32"), probe, "0", "probe-noirq"],
                       check=True, timeout=60)


if __name__ == "__main__":
    main()
