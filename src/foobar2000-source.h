#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool foobar2000_module_init(void);
void foobar2000_module_unload(void);

extern struct obs_source_info foobar2000_source_info;

#ifdef __cplusplus
}
#endif
