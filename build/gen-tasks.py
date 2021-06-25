from evergreen_config_lib.functions import all_functions
from evergreen_config_lib.tasks import all_tasks
from evergreen_config_lib.variants import all_variants

selected_variants = set()
selected_tasks = set()

for v in all_variants:
    for t in v.tasks:
        if "dns" in t:
            selected_variants.add (v.name)
            selected_tasks.add (t)
            print (f"{v.name} has task {t}")

patch_str = "evergreen patch --project=mongo-c-driver"
for v in selected_variants:
    patch_str += " -v " + v

for t in selected_tasks:
    patch_str += " -t " + t

print (patch_str)

