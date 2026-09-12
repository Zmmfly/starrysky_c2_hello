add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

includes(os.getenv("XHIVE_SDK_PATH"))

target("c2_hello")
    set_kind("binary")
    set_languages("c11")
    add_rules("xhive.embed")
    add_files("src/main.c")
    add_ldflags("-Wl,-Map=build/c2_hello.map", {force = true})
target_end()

task("flash")
    set_category("action")
    set_menu {
        usage = "xmake flash [options]",
        description = "Build and program C2 through HFP-LINK (Linux).",
        options = {
            {nil, "mount", "kv", nil, "Mounted YSYX-HFPLnk root; auto-detected by default."},
            {nil, "dry-run", "k", nil, "Build and validate without writing the device."}
        }
    }
    on_run(function ()
        import("core.base.json")
        import("core.base.option")
        import("core.base.task")
        import("core.project.config")
        import("core.project.project")

        assert(os.host() == "linux", "This board's flash task currently supports Linux")
        config.load()
        task.run("build", {target = "c2_hello"})
        local target = assert(project.target("c2_hello"))
        local binary = path.join(path.directory(target:targetfile()), "c2_hello.bin")
        local size = os.filesize(binary)
        assert(size and size > 0 and size <= 16 * 1024 * 1024,
               "Firmware must be nonempty and fit in the board's 16 MiB Flash")

        -- Query actual mounted filesystems, not directory names left behind
        -- after the physical switch disconnects the programmer.
        local output = os.iorunv("lsblk", {"--json", "--output", "LABEL,MOUNTPOINTS,RO"})
        local roots = {}
        local function collect(devices)
            for _, device in ipairs(devices) do
                if device.label == "YSYX-HFPLnk" and not device.ro then
                    for _, root in ipairs(device.mountpoints or {}) do
                        if type(root) == "string" then
                            roots[path.absolute(root)] = true
                        end
                    end
                end
                if type(device.children) == "table" then
                    collect(device.children)
                end
            end
        end
        collect(json.decode(output).blockdevices)

        local root = option.get("mount")
        if root then
            root = path.absolute(root)
            assert(roots[root], "--mount must name a writable mounted YSYX-HFPLnk root")
        else
            local count = 0
            for candidate in pairs(roots) do
                root = candidate
                count = count + 1
            end
            assert(count == 1, "Select HFP-LINK mode and mount its volume; use --mount if several exist")
        end
        local state = path.join(root, "STATE.TXT")
        assert(os.isfile(state), "HFP-LINK STATE.TXT is missing")
        -- Use an 8.3 filename to avoid extra long-filename FAT entries.
        local destination = path.join(root, "FIRMWARE.BIN")
        assert(not os.islink(state) and not os.islink(destination),
               "Programmer files must not be symbolic links")
        print("HFP-LINK: %s (%d bytes) -> %s", binary, size, destination)
        if option.get("dry-run") then
            print("Dry run: no device writes performed.")
            return
        end
        -- Do not copy source metadata or create a temporary file on the
        -- emulated FAT volume. Closing then syncfs flushes data and metadata.
        local image = assert(io.readfile(binary, {encoding = "binary"}))
        assert(#image == size, "Firmware changed or could not be read completely")
        local file = io.open(destination, "wb")
        file:write(image)
        file:flush()
        file:close()
        os.vrunv("sync", {"-f", root})
        local status = (io.readfile(state) or ""):trim()
        print("STATE.TXT: %s", status)
        assert(not status:lower():find("fail") and not status:lower():find("error"),
               "The programmer reported an error; keep HFP-LINK mode")
        print("Copy and filesystem sync completed; this is not Flash readback verification.")
        print("Wait for activity to stop, switch to UART/run mode, and reset the board.")
    end)
task_end()

task("monitor")
    set_category("action")
    set_menu {
        usage = "xmake monitor [options]",
        description = "Open the board's CP2102 serial console at 115200 8N1.",
        options = {
            {nil, "port", "kv", nil, "Serial device; auto-detect CP2102 by default."}
        }
    }
    on_run(function ()
        import("core.base.option")
        import("xhive.base")
        local python = base.detect_tool({"python3", "python"}, {check = "--version"})
        assert(python, "Python 3 and pyserial are required for the serial monitor")
        local port = option.get("port")
        if not port then
            local detected = os.iorunv(python.program, {"-c", [[
from serial.tools import list_ports
ports = [p.device for p in list_ports.comports() if (p.vid, p.pid) == (0x10c4, 0xea60)]
if len(ports) != 1:
    raise SystemExit("Switch to UART mode, or select a serial device with --port")
print(ports[0])
]]})
            port = detected:trim()
        end
        os.execv(python.program, {"-m", "serial.tools.miniterm", port, "115200", "--raw"})
    end)
task_end()
