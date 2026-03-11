# 2026-02-04T16:47:33.092747200
import vitis

client = vitis.create_client()
client.set_workspace(path="Desktop")

comp = client.create_app_component(name="lab1_part3",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

vitis.dispose()

