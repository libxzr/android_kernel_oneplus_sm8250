#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libfdt.h"
#include "libfdt_env.h"

void walk_device_tree(const void *device_tree, int parent_node) {
  int len = 0;
  const char *node_name = fdt_get_name(device_tree, parent_node, &len);
  if (node_name != NULL) {
    // avoid clang complaining about unused variable node_name and force
    // ASan to validate string pointer in strlen call.
    assert(strlen(node_name) == len);
  }

  uint32_t phandle = fdt_get_phandle(device_tree, parent_node);
  if (phandle != 0) {
    assert(parent_node == fdt_node_offset_by_phandle(device_tree, phandle));
  }

  // recursively walk the node's children
  for (int node = fdt_first_subnode(device_tree, parent_node); node >= 0;
       node = fdt_next_subnode(device_tree, node)) {
    walk_device_tree(device_tree, node);
  }
}

// Information on device tree is available in external/dtc/Documentation/
// folder.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Non-zero return values are reserved for future use.
  if (size < FDT_V17_SIZE) return 0;

  if (fdt_check_header(data) != 0) return 0;

  int root_node_offset = 0;
  walk_device_tree(data, root_node_offset);

  return 0;
}