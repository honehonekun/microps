#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#include "util.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "arp.h"

struct net_protocol {
    struct net_protocol *next;
    uint16_t type;
    net_protocol_handler_t handler;
};

struct net_protocol_queue_entry {
    struct queue_entry entry;
    struct net_device *dev;
    size_t len;
    /* data bytes exists after this structure. */
};

/*
 * NOTE: if you want to add/delete the entries after net_run(),
 *       you need to protect these lists with a lock.
 */

static struct net_device *devices;
static struct net_protocol *protocols;

struct net_device * net_device_alloc(void) //メモリ割り当て
{
    struct net_device *dev;
    dev = memory_alloc(sizeof(*dev));
    if (!dev)
    {
        errorf("memory_alloc() failure");
        return NULL;
    }
    return dev; //ポインタを返す
}

/*
 * NOTE: must not be call after net_run()
 */
int net_device_register(struct net_device *dev) //リストにデバイスを登録
{
    static unsigned int index = 0; //staticなので値が保持される

    dev->index = index++; //使った後インクリメント
    snprintf(dev->name, sizeof(dev->name), "net%d", dev->index); //名前書き込み
    dev->next = devices; //nextに最初はnullを登録
    devices = dev; //次呼び出されたら自分がnextに入る
    infof("success,dev=%s,type=0x%04x", dev->name, dev->type);
    return 0;
}

static int
net_device_open(struct net_device *dev) //デバイスにオープンフラグをセット
{
    infof("dev=%s",dev->name);
    if(NET_DEVICE_IS_UP(dev)){
        errorf("already opened,dev=%s",dev->name);
        return -1;
    }
    if(dev->ops->open){
        if(dev->ops->open(dev)==-1){
            errorf("failure,dev=%s",dev->name);
            return -1;
        }

    }

    dev->flags |= NET_DEVICE_FLAG_UP;
    return 0;

}

static int
net_device_close(struct net_device *dev) //デバイスにクローズフラグをセット
{
    infof("dev=%s",dev->name);
    if(!NET_DEVICE_IS_UP(dev)){
        errorf("not opened,dev=%s",dev->name);
        return -1;
    }
    if(dev->ops->close){
        if(dev->ops->close(dev)==-1){
            errorf("failure,dev=%s",dev->name);
            return -1;
        }
    }
    dev->flags &= ~NET_DEVICE_FLAG_UP;
    return 0;
}

int net_device_output(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    //送信処理
    debugf("dev=%s,type=0x%04x,len=%zu",dev->name,type,len);
    debugdump(data,len); //コンパイル時に宣言したら表示される
    if(!NET_DEVICE_IS_UP(dev)){
        errorf("not opened,dev=%s",dev->name);
        return -1;
    }
    if(dev-> mtu <len){
        errorf("too long, dev=%s,mtu=%u,len=%zu",dev->name,dev->mtu,len);
        return -1;
    }

    if(!dev->ops->output){
        errorf("output callback function is not set, dev=%s",dev->name);
        return -1;
    }

    if(dev->ops->output(dev,type,data,len,dst)==-1){
        errorf("failure,dev=%s,len=%zu",dev->name,len);
        return -1;
    }

    return 0;
}

/*
 * NOTE: must not be call after net_run()
 */
int
net_device_add_iface(struct net_device *dev, struct net_iface *iface)
{
    struct net_iface *entry;

    for(entry = dev-> ifaces;entry;entry = entry-> next){
        if(entry->family == iface->family){
            errorf("already exisits,dev=%s,family=%d",dev->name,entry->family);
            return -1;
        }
    }
    iface -> next = dev->ifaces;
    iface->dev = dev;
    dev->ifaces = iface;
    infof("success,dev=%s",dev->name);
    return 0;
}

struct net_iface *
net_device_get_iface(struct net_device *dev, int family) //familyのインタフェースを探す
{
    struct net_iface *entry;
    for(entry = dev->ifaces;entry;entry=entry->next){
        if(entry->family == family){
            break;
        }
    }
    return entry;
}

/*
 * NOTE: must not be call after net_run()
 */
int
net_protocol_register(uint16_t type, net_protocol_handler_t handler)
{
    struct net_protocol *proto;

    for(proto =protocols;proto;proto=proto->next){
        if(type==proto->type){
            errorf("already registered type=0x%04x",proto->type);
            return -1;
        }
    }
    proto = memory_alloc(sizeof(*proto));
    if(!proto){
        errorf("memory_alloc() failure");
        return -1;
    }
    proto ->type= type;
    proto->handler = handler;
    proto->next=protocols;
    protocols = proto;
    infof("success,type=0x%04x",type);
    return 0;
}

static struct net_protocol_queue_entry *
net_protocol_queue_push(struct net_protocol *proto, const uint8_t *data, size_t len, struct net_device *dev)
{
}

static struct net_protocol_queue_entry *
net_protocol_queue_pop(struct net_protocol *proto)
{
}

static struct net_protocol_queue_entry *
net_protocol_queue_push(struct net_protocol *proto, const uint8_t *data, size_t len, struct net_device *dev)
{
}

static struct net_protocol_queue_entry *
net_protocol_queue_pop(struct net_protocol *proto)
{
}

int
net_input(uint16_t type, const uint8_t *data, size_t len, struct net_device *dev)
{
    struct net_protocol *proto;

    debugf("dev=%s,type=0x%04x len=%zu",dev->name,type,len);
    debugdump(data,len);
    for(proto = protocols;proto;proto=proto->next){
        if(proto->type == type){
            proto->handler(data,len,dev);
            return 0;
        }
    }
    return 0;
}

void
net_softirq_handler(unsigned int irq, void *arg)
{
}

#include "arp.h"
#include "ip.h"
#include "icmp.h"

int
net_init(void)
{
    infof("initialize...");
    if (platform_init() == -1)
    {
        errorf("platform_init() failure");
        return -1;
    }
    if(arp_init() == -1){
        errorf("arp_init() failure");
        return -1;
    }


    if(ip_init() == -1){
        errorf("ip_init() failure");
        return -1;
    }
    if(icmp_init() == -1){
        errorf("icmp_init() failure");
        return -1;
    }
    infof("success");
    return 0;
}

int net_run(void) //登録済みデバイスを順番に実行可能にする
{
    struct net_device *dev;
    infof("startup...");
    if (platform_run() == -1)
    {
        errorf("platform_run() failure");
        return -1;
    }
    for(dev = devices;dev;dev = dev->next){ //真ん中はnullなら偽として扱われる
        net_device_open(dev);
    }
    infof("success");

    return 0;
}

int net_shutdown(void)
{
    struct net_device *dev;

    infof("shutting down...");
    if (platform_shutdown() == -1)
    {
        warnf("platform_shutdown() failure");
    }
    for (dev = devices;dev;dev = dev->next){
        net_device_close(dev);
    }
    infof("success");
    return 0;
}
