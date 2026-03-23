# 2026-02-04T15:33:28.889135700
import vitis

client = vitis.create_client()
client.set_workspace(path="Desktop")

platform = client.get_component(name="lab1_platform")
status = platform.build()

comp = client.get_component(name="lab1_part2")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

comp = client.get_component(name="lab1_part2")
status = comp.import_files(from_loc="$COMPONENT_LOCATION/../lab1_part1", files=["pmodkypd.c", "pmodkypd.h"], dest_dir_in_cmp = "lab1_part2")

status = platform.build()

comp = client.get_component(name="lab1_part2")
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

comp = client.clone_component(name="lab1_part2",new_name="lab1_part3")

status = platform.build()

comp = client.get_component(name="lab1_part3")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

client.delete_component(name="lab1_part3")

comp = client.create_app_component(name="lab1_part3",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab1_part3")
status = comp.import_files(from_loc="", files=["C:\Users\diepreye\Desktop\lab1_part2\pmodkypd.c", "C:\Users\diepreye\Desktop\lab1_part2\pmodkypd.h", "C:\Users\diepreye\Desktop\lab1_part2\rgb_led.h"])

status = platform.build()

comp = client.get_component(name="lab1_part3")
comp.build()

status = platform.build()

comp.build()

client.delete_component(name="lab1_part3")

comp = client.create_app_component(name="lab1_part3",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

status = platform.build()

comp.build()

comp = client.get_component(name="lab1_part3")
status = comp.import_files(from_loc="$COMPONENT_LOCATION/../lab1_part2", files=["pmodkypd.c", "pmodkypd.h", "rgb_led.h"], dest_dir_in_cmp = "lab1_part3")

status = platform.build()

comp = client.get_component(name="lab1_part3")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = comp.clean()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

client.delete_component(name="lab1_part3")

vitis.dispose()

