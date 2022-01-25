﻿./config.ps1

# This is an example of the out from the azsphere image add command that will be parsed below for the image id
# upload_image = "Uploading image from file '.\DivvyInterfaceBox.imagepackage':  --> Image ID:       6dd85b6b-406b-43a8-9e89-a6e8cb51130c  --> Component ID:   bdd4f26f-468b-4497-aad9-e3a4e7280025  --> Component name: 'DivvyInterfaceBox' Removing temporary state for uploaded image. Successfully uploaded image with ID '6dd85b6b-406b-43a8-9e89-a6e8cb51130c' and name 'DivvyInterfaceBox' to component with ID 'bdd4f26f-468b-4497-aad9-e3a4e7280025'."

write-host "`n"
write-host "Uploading image to tenant id: $tenant_id"

$upload_image = azsphere image add --image $global:image_path_filename -t $tenant_id

$i = $upload_image.Split(">").Trim()

# This is where you'll find the image id in the image upload return string
$image_id = $i[2].split(":")[1].Trim()


write-host "`nCreating deployment for device group id: $device_group on tenant id: $tenant_id for image id: $image_id"

azsphere device-group deployment create --device-group $device_group --images $image_id -t $tenant_id

write-host "`nImage details for image id: $image_id"

azsphere image show -i 2e7003a4-a848-4eca-8c1c-4cd74d8ec1d4

write-host "`nList of all images for device group id: $device_group on tenant id: $tenant_id"

azsphere device-group deployment list --device-group $device_group -t $tenant_id