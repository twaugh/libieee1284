/*
 * Driver for IO port access
 *
 * PC-DOS compatibility requirements:
 *      Nearly all locations have defined values, see
 *      the PC AT Hardware reference manual for details.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#if 0
#include <sys/dir.h>
#endif
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static void *state_head;  /* opaque handle top of state structs */

/* Prototypes */

static int
iopattach(dev_info_t *dip, ddi_attach_cmd_t cmd);

static int
iopgetinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp);

static int 
iopdetach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static int
iopopen(dev_t *devp, int flag, int otyp, cred_t *credp);

static int
iopclose(dev_t dev, int openflags, int otyp, cred_t *credp);

static int
iopioctl(dev_t dev, 
         int cmd, 
         intptr_t arg, 
         int mode, 
         cred_t *credp,
         int *rvalp);

/* device operations */
#define IOPREAD 1
#define IOPWRITE 2

typedef struct iop_struct {         /* per-unit structure */
    dev_info_t *dip;
    kmutex_t mutex;
} iop;

typedef struct iopbuf_struct{
    unsigned int port;
    unsigned char port_value;
} iopbuf;

static struct cb_ops iop_cb_ops = {
    iopopen,
    iopclose,
    nodev,                 /* not a block driver */
    nodev,                 /* no print */
    nodev,                 /* no dump */
    nodev,                 /* no read */
    nodev,                 /* no write */
    iopioctl,
    nodev,                 /* no devmap */
    nodev,                 /* no mmap */
    nodev,                 /* no segmap */
    nochpoll,              /* no chpoll */
    ddi_prop_op,
    0,                     /* not a STREAMS driver */
    D_NEW | D_MP,          /* (hopefully) MT and MP safe */
};

static struct dev_ops iop_ops = 
{
    DEVO_REV,              /* devo_rev */
    0,                     /* devo_refcnt */
    iopgetinfo,            /* devo_getinfo */
    nulldev,               /* devo_identify */
    nulldev,               /* devo_probe */
    iopattach,             /* devo_attach */
    iopdetach,             /* devo_detach */
    nodev,                 /* devo_reset */
    &iop_cb_ops,           /* devo_cb_ops */
    (struct bus_ops *)0,   /* devo_bus_ops */
    nodev                  /* devo_power */
};

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
    &mod_driverops,
    "Simon's IO Port driver 0.9",
    &iop_ops
};
    
static struct modlinkage modlinkage = {
    MODREV_1, 
    {(void *)&modldrv, NULL,}
};

/* Globally exported functions */
int
_init(void)
{
    int error;
    
#ifdef DEBUG
    cmn_err(CE_CONT, "_init: start\n");
#endif
    if ((error = ddi_soft_state_init(&state_head, sizeof(iop), 1)) != 0) {
#ifdef DEBUG
        cmn_err(CE_CONT, "_init: couldn't ddi_soft_state_init\n");
#endif
        return error;
    }
    if ((error = mod_install(&modlinkage)) != 0) {
#ifdef DEBUG
        cmn_err(CE_CONT, "_init: couldn't mod_install\n");
#endif
        ddi_soft_state_fini(&state_head);
    }
#ifdef DEBUG
    cmn_err(CE_CONT, "_init: done\n");
#endif

    return error;
}

int
_info(struct modinfo *modinfop)
{
    int retval;
    
#ifdef DEBUG
    cmn_err(CE_CONT, "_info: start\n");
#endif
    retval = mod_info(&modlinkage, modinfop);
#ifdef DEBUG
    cmn_err(CE_CONT, "_info: done\n");
#endif
    return retval;
}

int
_fini(void)
{
    int status;
    
#ifdef DEBUG
    cmn_err(CE_CONT, "_fini: start\n");
#endif
    if ((status = mod_remove(&modlinkage)) != 0) {
#ifdef DEBUG
        cmn_err(CE_CONT, "_fini: couldn't mod_remove\n");
#endif
        return status;
    }
    ddi_soft_state_fini(&state_head);
#ifdef DEBUG
    cmn_err(CE_CONT, "_fini: done\n");
#endif
    return status;
}

static int
iopattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
    int instance;
    iop *iop_p;
    
    switch(cmd) {
    case DDI_ATTACH:
        break;
    default:
        return DDI_FAILURE;
    }
    instance = ddi_get_instance(dip);
#ifdef DEBUG
    cmn_err(CE_CONT, "iopattach: start, instance = %d\n", instance);
#endif
    if (ddi_soft_state_zalloc(state_head, instance) != 0) {
#ifdef DEBUG
        cmn_err(CE_CONT, "iopattach: ddi_soft_state_zalloc failed\n");
#endif
        return DDI_FAILURE;
    }
    iop_p = (iop *)ddi_get_soft_state(state_head, instance);
    ddi_set_driver_private(dip, (caddr_t)iop_p);
    iop_p->dip = dip;
    mutex_init(&iop_p->mutex, "iop mutex", MUTEX_DRIVER, (void *)0);
    if (ddi_create_minor_node(dip, ddi_get_name(dip), S_IFCHR, instance,
                              DDI_PSEUDO, 0) == DDI_FAILURE)
    {
        mutex_destroy(&iop_p->mutex);
        ddi_soft_state_free(state_head, instance);
#ifdef DEBUG
        cmn_err(CE_CONT, "iopattach: ddi_create_minor_node failed\n");
#endif
        return DDI_FAILURE;
    }
    ddi_report_dev(dip);
#ifdef DEBUG
    cmn_err(CE_CONT, "iopattach: done\n");
#endif
    return DDI_SUCCESS;
}

static int
iopgetinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
    int error;
    iop *iop_p;
        
#ifdef DEBUG
    cmn_err(CE_CONT, "iopgetinfo: start\n");
#endif
    switch(cmd) {
    case DDI_INFO_DEVT2DEVINFO:
        iop_p = (iop *)ddi_get_soft_state(state_head, 
                                          getminor((dev_t)arg));
        if (iop_p == NULL) {
            *resultp = NULL;
#ifdef DEBUG
            cmn_err(CE_CONT, "iopgetinfo: ddi_get_soft_state failed\n");
#endif
            error = DDI_FAILURE;
        } else {
            mutex_enter(&iop_p->mutex);
            *resultp = iop_p->dip;
            mutex_exit(&iop_p->mutex);
            error = DDI_SUCCESS;
        }
        break;
    case DDI_INFO_DEVT2INSTANCE:
        *resultp = (void *)getminor((dev_t)arg);
        error = DDI_SUCCESS;
        break;
    default:
        *resultp = NULL;
        return DDI_FAILURE;
    }
#ifdef DEBUG
    cmn_err(CE_CONT, "iopgetinfo: done\n");
#endif
    return error;
}

static int 
iopdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
    iop *iop_p;
    int instance;
    
#ifdef DEBUG
    cmn_err(CE_CONT, "iopdetach: start\n");
#endif
    switch(cmd) {
    case DDI_DETACH:
        break;
    default:
        return DDI_FAILURE;
    }
    
    instance = ddi_get_instance(dip);
    iop_p = (iop *)ddi_get_soft_state(state_head, instance);
    ddi_remove_minor_node(dip, NULL);
    mutex_destroy(&iop_p->mutex);
    ddi_soft_state_free(state_head, instance);
#ifdef DEBUG
    cmn_err(CE_CONT, "iopdetach: done\n");
#endif
    return DDI_SUCCESS;
}

static int
iopopen(dev_t *devp, int flag, int otyp, cred_t *credp)
{
    int retval = 0;
    iop *iop_p;
    
    iop_p = (iop *)ddi_get_soft_state(state_head, getminor(*devp));
    
    if (iop_p == NULL) {
        return ENXIO;
    }
    
    if (otyp != OTYP_CHR) {
        return EINVAL;
    }
  
    if ((flag & FWRITE) &&
        (drv_priv(credp) != 0)) 
    {
        retval = EACCES;
    } else {
        retval = 0;
    }
    return retval;
}

static int
iopclose(dev_t dev, int openflags, int otyp, cred_t *credp)
{
    iop *iop_p;
    
    iop_p = (iop *)ddi_get_soft_state(state_head, getminor(dev));
    return 0;
}

/* cmd should be IOPREAD or IOPWRITE
   arg is the address (of the io port)
   */
int
iopioctl(dev_t dev, 
         int cmd, 
         intptr_t arg, 
         int mode, 
         cred_t *credp,
         int *rvalp)
{
    iop *iop_p;
    int retval = 0;
    iopbuf tmpbuf;
    
    iop_p = (iop *)ddi_get_soft_state(state_head, getminor(dev));

    /* we will have to check privileges once we do writes */
    switch (cmd) {
    case IOPREAD:
        mutex_enter(&iop_p->mutex);
        if (ddi_copyin((caddr_t)arg, 
                       (caddr_t)&tmpbuf, 
                       sizeof(tmpbuf),
                       mode)) 
        {
            retval = EFAULT;
        } else {
            tmpbuf.port_value = inb(tmpbuf.port);
            ddi_copyout((caddr_t)&tmpbuf.port_value, 
                        (caddr_t)(arg+sizeof(tmpbuf.port)),
                        sizeof(tmpbuf.port),
                        mode);
        }
        mutex_exit(&iop_p->mutex);
        break;
    case IOPWRITE:
        mutex_enter(&iop_p->mutex);
        if (ddi_copyin((caddr_t)arg,
                       (caddr_t)&tmpbuf,
                       sizeof(tmpbuf),
                       mode)) 
        {
            retval = EFAULT;
        } else {
            outb(tmpbuf.port,tmpbuf.port_value);
        }
        mutex_exit(&iop_p->mutex);
        break;
    default:
        retval = EINVAL;
        break;
    }
    return retval;
    
}
