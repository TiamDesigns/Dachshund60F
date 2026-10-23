/*
 * Config protocol service: answers requests from the host on the config
 * interface (protocol/protocol.yaml), using kb/protocol.h from the core.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* Start the request thread and listen on the USB config interface. */
int config_start(void);

#endif /* CONFIG_H */
