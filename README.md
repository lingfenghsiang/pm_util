# pm_util: Tool to collect Optane DC persistent memory read/write access

## Requirements

This library uses "ipmctl" to get PM DIMM data. You need to run your code as "root" while using this library. On older systems, ipmctl may not apply. You need to use "ipmwatch", which is included in Vtune tool by Intel.

## How to compile

```
mkdir build
cd build && cmake ..
make
```

## How to use
An example is shown below. A program writes 1GB. By creating a util::PmmDataCollector object, the data will be loaded in the float variables.

Original code:

```
char* an_address_on_persistent_memory;    
memset(&an_address_on_persistent_memory, 0, (1ULL<<30));
```
Insert the library:
```
#include "include/pm_util.h"

float imc_rd, imc_wr, media_rd, media_wr;
char* an_address_on_persistent_memory;
{
    util::PmmDataCollector measure("PM data", &imc_rd, &imc_wr, &media_rd, &media_wr);
    memset(&an_address_on_persistent_memory, 0, (1ULL<<30));
}
std::cout <<  imc_rd << imc_wr << media_rd << media_wr << std::endl;
```