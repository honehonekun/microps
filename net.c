#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#include "util.h"
#include "net.h"

/*
 * NOTE: if you want to add/delete the entries after net_run(),
 *       you need to protect these lists with a lock.
 */
static struct net_device *devices; //最初はnull 共有リスト

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
    dev->flags &= ~NET_DEVICE_FLAG_UP;
    return 0;
}

int net_device_output(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    //送信処理（今はダンプだけ）
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
    return 0;
}

int
net_input(uint16_t type, const uint8_t *data, size_t len, struct net_device *dev)
{
}

int
net_init(void)
{
    infof("initialize...");
    if (platform_init() == -1)
    {
        errorf("platform_init() failure");
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
