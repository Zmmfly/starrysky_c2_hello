add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
-- With ENABLE_IRQ_QREGS=0, IRQ entry overwrites gp/tp. Reserve both in
-- application, Nano and driver code; linker relaxation must not use gp.
add_cxflags("-ffixed-x3", "-ffixed-x4", "-msmall-data-limit=0", {force = true})
add_ldflags("-Wl,--no-relax", {force = true})
includes(os.getenv("XHIVE_SDK_PATH"))
set_targetdir("dist")

target("c2_rtthread")
    set_kind("binary")
    set_languages("c11")
    add_rules("xhive.embed")
    add_files("src/*.c", "src/*.S")
    add_ldflags("-Wl,-Map=build/c2_rtthread.map", {force = true})
target_end()
