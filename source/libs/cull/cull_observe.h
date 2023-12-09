#include "cull/cull_list.h"

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef OBSERVE

void lObserveInit(void);
void lObserveStart(void);
void lObserveEnd(void);

void lObserveAdd(const void *pointer, const void *owner, bool is_list);
void lObserveRemove(const void *pointer);

void lObserveSwitchOwner(const void *pointer_a, const void *pointer_b, const void *owner_a, const void *owner_b, int nm);
void lObserveChangeOwner(const void *pointer, const void *new_owner, const void *old_owner, int nm);
void lObserveChangeValue(const void *pointer, bool has_hash, int nm);
void lObserveChangeListType(const void *pointer, bool is_master_list, const char *list_name);

void lObserveGetInfoString(dstring *dstr);
long lObserveGetSize(void);

#endif 

#ifdef  __cplusplus
}
#endif

