# Contract: descriptor update templates on the wire (047)

## What the generator emits

A command whose `pData` is sized by a descriptor update template gets its blob
length from a hook, the way `VK_EXT_host_image_copy` gets its byte count
(`tools/codegen/rvk_codegen/emit/serialize.py`):

```cpp
uint64_t descriptor_template_bytes(uint64_t template_handle);
```

declared in the generated header, with a weak default of 0 in a generated
translation unit, and a strong definition on each side:

- guest: `src/icd/vulkan/icd_hooks.cpp`, answering from `DescriptorTemplateTable`;
- host: `src/device/executor/vulkan_backend.cpp`, answering from `HostTemplateTable`.

A length of 0 means "no bytes to carry", which is what an empty template
means; it is not an error, and the command still crosses.

## Which commands it carries

| Command | Extension | Blob |
|---|---|---|
| `vkUpdateDescriptorSetWithTemplate` | core 1.1 | one update's descriptors |
| `vkUpdateDescriptorSetWithTemplateKHR` | `VK_KHR_descriptor_update_template` | the same, aliased |
| `vkCmdPushDescriptorSetWithTemplateKHR` | `VK_KHR_push_descriptor` | the same |

`vkCreateDescriptorUpdateTemplate` and its `KHR` alias are already generated;
they gain no new wire shape, only the two tables that remember what they made.

## What each side must do

1. **The guest records before it sends.** The create command's entries are
   copied into `DescriptorTemplateLayout` before the template handle is
   returned to the application, so a later update can be sized. A create the
   host refused records nothing.
2. **The guest sends the blob's length with it**, as every other sized array
   does; the length it sends is the one its own layout answered.
3. **The host checks the length against its own layout** and refuses a frame
   whose blob is shorter or longer than the template it names accounts for.
   The guest's number is a claim, not a fact (constitution V).
4. **Neither side reads a descriptor out of the blob.** The bytes are carried
   whole and handed to the host driver; what is in them is the application's.

## Refusals

| Situation | Answer |
|---|---|
| a template handle neither side made | the frame is refused, `Status::not_found` |
| entries whose end overflows 64 bits | the create is refused, `VK_ERROR_INITIALIZATION_FAILED` |
| a blob length the layout does not account for | the frame is refused |
| an update naming a destroyed template | refused; the table forgets on destroy |
