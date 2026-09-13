add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
-- With ENABLE_IRQ_QREGS=0, IRQ entry overwrites gp/tp. Reserve both in
-- application, Nano and driver code; linker relaxation must not use gp.
add_cxflags("-ffixed-x3", "-ffixed-x4", "-msmall-data-limit=0", {force = true})
add_ldflags("-Wl,--no-relax", {force = true})
includes(os.getenv("XHIVE_SDK_PATH"))
set_targetdir("dist")

-- The SDK Nano target has no PicoRV32 port. Keep its kernel/config generator,
-- selecting project board/CPU code instead of the incompatible CSR port.
target("xhive_embed::rttnano")
    on_load(function (target)
        local nano = path.join(os.getenv("XHIVE_SDK_PATH"), "third-party/rttnano")
        target:add("includedirs", path.join(nano, "include"),
                   path.join(nano, "components/finsh"), {public = true})
        target:add("files", path.join(nano, "src/*.c"))
        for _, file in ipairs({"shell.c", "msh.c", "msh_parse.c", "cmd.c"}) do
            target:add("files", path.join(nano, "components/finsh", file))
        end
    end)
target_end()

target("c2_rtthread")
    set_kind("binary")
    set_languages("c11")
    add_rules("xhive.embed")
    add_files("src/*.c", "src/*.S")
    add_ldflags("-Wl,-Map=build/c2_rtthread.map", {force = true})
target_end()
