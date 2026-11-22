#include <sys/prx_module.h>

PRX_LIBRARY_BEGIN(ghosts_pad_probe, "ghosts_pad_probe")
    PRX_EXPORT_FUNC(ghosts_probe_ping, 0x47505242)
PRX_LIBRARY_END(ghosts_pad_probe)
