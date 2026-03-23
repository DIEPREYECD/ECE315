# 2026-02-04T16:49:21.869205900
import vitis

client = vitis.create_client()
client.set_workspace(path="Desktop")

platform = client.get_component(name="lab1_platform")
status = platform.build()

comp = client.get_component(name="lab1_part3")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp = client.get_component(name="lab1_part2")
comp.build()

vitis.dispose()

