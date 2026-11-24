#include <sys/prx_module.h>

PRX_LIBRARY_BEGIN(ghosts_pad_input_probe, "ghosts_pad_input_probe")
    PRX_EXPORT_FUNC(ghosts_input_probe_ping, 0x47504950)
PRX_LIBRARY_END(ghosts_pad_input_probe)
