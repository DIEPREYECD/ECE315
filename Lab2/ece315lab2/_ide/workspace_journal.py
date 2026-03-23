# 2026-03-04T13:45:59.019260400
import vitis

client = vitis.create_client()
client.set_workspace(path="ece315lab2")

platform = client.create_platform_component(name = "lab2platform",hw_design = "$COMPONENT_LOCATION/../../lab1_hw/ece315_lab1/lab1_hw_wrapper.xsa",os = "freertos",cpu = "ps7_cortexa9_0",domain_name = "freertos_ps7_cortexa9_0")

platform = client.get_component(name="lab2platform")
domain = platform.get_domain(name="freertos_ps7_cortexa9_0")

status = domain.set_config(option = "os", param = "freertos_tick_rate", value = "1000")

status = domain.regenerate()

status = platform.build()

comp = client.create_app_component(name="lab2part1",platform = "$COMPONENT_LOCATION/../lab2platform/export/lab2platform/lab2platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab2part1")
status = comp.import_files(from_loc="", files=["C:\Users\diepreye\Desktop\lab2_part1\lab2_part1.c", "C:\Users\diepreye\Desktop\lab2_part1\sha256.c", "C:\Users\diepreye\Desktop\lab2_part1\sha256.h"])

status = platform.build()

comp = client.get_component(name="lab2part1")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

comp = client.create_app_component(name="lab2part2",platform = "$COMPONENT_LOCATION/../lab2platform/export/lab2platform/lab2platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab2part2")
status = comp.import_files(from_loc="", files=["C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part2\lab1_part3.c", "C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part2\pmodkypd.c", "C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part2\pmodkypd.h", "C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part2\rgb_led.h"])

status = platform.build()

comp = client.get_component(name="lab2part2")
comp.build()

comp = client.create_app_component(name="lab2part3",platform = "$COMPONENT_LOCATION/../lab2platform/export/lab2platform/lab2platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab2part3")
status = comp.import_files(from_loc="", files=["C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part3\lab2_part3.c", "C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part3\uart_driver.c", "C:\Users\diepreye\Downloads\ece315lab2\ece315lab2\lab2part3\uart_driver.h"])

status = platform.build()

comp = client.get_component(name="lab2part3")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp = client.get_component(name="lab2part2")
comp.build()

status = platform.build()

comp = client.get_component(name="lab2part3")
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

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp = client.get_component(name="lab2part2")
comp.build()

status = platform.build()

comp = client.get_component(name="lab2part3")
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

vitis.dispose()

