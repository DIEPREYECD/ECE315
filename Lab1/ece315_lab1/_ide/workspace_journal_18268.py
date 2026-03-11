# 2026-02-04T14:23:41.394475
import vitis

client = vitis.create_client()
client.set_workspace(path="Desktop")

platform = client.create_platform_component(name = "lab1_platform",hw_design = "$COMPONENT_LOCATION/../../Downloads/lab1_hw/ece315_lab1/lab1_hw_wrapper.xsa",os = "freertos",cpu = "ps7_cortexa9_0",domain_name = "freertos_ps7_cortexa9_0")

comp = client.create_app_component(name="lab1_part1",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab1_part1")
status = comp.import_files(from_loc="", files=["C:\Users\diepreye\Downloads\lab1_src\part1\lab1_part1.c", "C:\Users\diepreye\Downloads\lab1_src\part1\pmodkypd.c", "C:\Users\diepreye\Downloads\lab1_src\part1\pmodkypd.h"])

platform = client.get_component(name="lab1_platform")
status = platform.build()

comp = client.get_component(name="lab1_part1")
comp.build()

domain = platform.get_domain(name="freertos_ps7_cortexa9_0")

status = domain.set_config(option = "os", param = "freertos_tick_rate", value = "1000")

status = domain.regenerate()

status = platform.build()

status = domain.regenerate()

status = platform.build()

status = platform.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

comp = client.create_app_component(name="lab1_part2",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab1_part2")
status = comp.import_files(from_loc="", files=["C:\Users\diepreye\Desktop\lab1_part1\lab1_part1.c"])

status = comp.import_files(from_loc="$COMPONENT_LOCATION/../../Downloads/lab1_src/part2", files=["rgb_led.h"], dest_dir_in_cmp = "lab1_part2")

vitis.dispose()

