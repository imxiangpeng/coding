

gcc -g j2s.c j2sobject.c hrserver_object.c -I. `pkg-config --cflags libcjson` `pkg-config --libs libcjson` -o j2s

gcc -g j2sobject.c mactbl_array.c -I. `pkg-config --cflags libcjson` `pkg-config --libs libcjson` -o mactbl_array

gcc -g j2sobject.c j2s_test_number_array.c -I. `pkg-config --cflags libcjson` `pkg-config --libs libcjson` -o number_array

gcc -g j2sobject.c j2sobject_string_array.c -I. `pkg-config --cflags libcjson` `pkg-config --libs libcjson` -o string_array


# 查看运行结果
./j2s
valgrind --leak-check=full ./j2s



20231229. 支持结构体中子结构体， prototype 中添加了构造以及init 和析构函数

typedef struct hrobject_ip {
    J2COBJECT_DECLARE_OBJECT;
    char address[HR_IP_ADDRESS_LEN];
    char gateway[HR_IP_ADDRESS_LEN];
    char netmask[HR_IP_ADDRESS_LEN];
    char dns[HR_IP_ADDRESS_LEN];
    char dns2[HR_IP_ADDRESS_LEN];
} hrobject_ip;


typedef struct hrobject_wan {
    J2COBJECT_DECLARE_OBJECT;
    struct hr_wan data;
    hrobject_ip v4;
} hrobject_wan;

// clang-format off
// must special offset_len when you using char array
static struct j2cobject_prototype hrobject_ip_prototype[] = {
    {.name = "address",              .type = J2C_STRING,    .offset = _J2COBJECT_IP_DATA_OFFSET(address),              .offset_len = _J2COBJECT_IP_DATA_LEN(address)},
    {.name = "gateway",              .type = J2C_STRING,    .offset = _J2COBJECT_IP_DATA_OFFSET(gateway),              .offset_len = _J2COBJECT_IP_DATA_LEN(gateway)},
    {.name = "netmask",              .type = J2C_STRING,    .offset = _J2COBJECT_IP_DATA_OFFSET(netmask),              .offset_len = _J2COBJECT_IP_DATA_LEN(netmask)},
    {.name = "dns",                  .type = J2C_STRING,    .offset = _J2COBJECT_IP_DATA_OFFSET(dns),              .offset_len = _J2COBJECT_IP_DATA_LEN(dns)},
    {.name = "dns2",                 .type = J2C_STRING,    .offset = _J2COBJECT_IP_DATA_OFFSET(dns2),              .offset_len = _J2COBJECT_IP_DATA_LEN(dns2)},
    {0}
};

struct j2cobject *hrobject_ip_allocate() {
    struct j2cobject *obj = j2cobject_create(J2C_OBJECT, sizeof(hrobject_ip));
    if (!obj)
        return NULL;
    obj->name = "ip";
    obj->prototype = hrobject_ip_prototype;
    return obj;
}
int hrobject_ip_setup(struct j2cobject *obj) {
    obj->name = "ip";
    obj->prototype = hrobject_ip_prototype;
    
    printf("%s(%d): object :%p\n", __FUNCTION__, __LINE__, obj);
    // should not clear memory, only setup prototype
    // 这里不允许清空后面内存，因为会导致数据丢失，我们仅仅安装 prototype
    //memset((void*)(obj+1), 0, sizeof(hrobject_ip) - sizeof(struct j2cobject));
    printf("%s(%d): object :%p, proto:%p\n", __FUNCTION__, __LINE__, obj, obj->prototype);

    return 0;
}
int hrobject_ip_deallocate(struct j2cobject *obj) {
    j2cobject_free(obj);
    
    return 0;
}

static struct j2cobject_prototype hrobject_wan_prototype[] = {
    {.name = "index",             .type = J2C_INT,       .offset = _J2COBJECT_WAN_DATA_OFFSET(index),             .offset_len = 0},
    {.name = "name",              .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(name),              .offset_len = _J2COBJECT_WAN_DATA_LEN(name)},
    {.name = "workmode",          .type = J2C_INT,       .offset = _J2COBJECT_WAN_DATA_OFFSET(workmode),          .offset_len = 0},
    {.name = "mac",               .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(mac),               .offset_len = _J2COBJECT_WAN_DATA_LEN(mac)},
    {.name = "pppoe_username",    .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(pppoe_username),    .offset_len = _J2COBJECT_WAN_DATA_LEN(pppoe_username)},
    {.name = "pppoe_password",    .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(pppoe_password),    .offset_len = _J2COBJECT_WAN_DATA_LEN(pppoe_password)},
    {.name = "v4_proto",          .type = J2C_INT,       .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_proto),          .offset_len = 0 },
    {.name = "v4_status",         .type = J2C_INT,       .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_status),         .offset_len = 0},
    {.name = "v4_address",        .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_address),        .offset_len = _J2COBJECT_WAN_DATA_LEN(v4_address)},
    {.name = "v4_gateway",        .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_gateway),        .offset_len = _J2COBJECT_WAN_DATA_LEN(v4_gateway)},
    {.name = "v4_netmask",        .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_netmask),        .offset_len = _J2COBJECT_WAN_DATA_LEN(v4_netmask)},
    {.name = "v4_dns",            .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_dns),            .offset_len = _J2COBJECT_WAN_DATA_LEN(v4_dns)},
    {.name = "v4_dns2",           .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_dns2),           .offset_len = _J2COBJECT_WAN_DATA_LEN(v4_dns2)},
    {.name = "v6_proto",          .type = J2C_INT,       .offset = _J2COBJECT_WAN_DATA_OFFSET(v4_proto),          .offset_len = 0},
    {.name = "v6_address",        .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v6_address),        .offset_len = _J2COBJECT_WAN_DATA_LEN(v6_address)},
    {.name = "v6_gateway",        .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v6_gateway),        .offset_len = _J2COBJECT_WAN_DATA_LEN(v6_gateway)},
    {.name = "v6_netmask",        .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v6_netmask),        .offset_len = _J2COBJECT_WAN_DATA_LEN(v6_netmask)},
    {.name = "v6_dns",            .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v6_dns),            .offset_len = _J2COBJECT_WAN_DATA_LEN(v6_dns)},
    {.name = "v6_dns2",           .type = J2C_STRING,    .offset = _J2COBJECT_WAN_DATA_OFFSET(v6_dns2),           .offset_len = _J2COBJECT_WAN_DATA_LEN(v6_dns2)},
    {.name = "v4",           .type = J2C_OBJECT,    .offset = offsetof(hrobject_wan, v4),           .offset_len = sizeof(hrobject_ip), hrobject_ip_allocate, hrobject_ip_init, hrobject_ip_deallocate},
    {0}
}


PS.20240206, 开始支持数组对象，仅限顶层对象数组，例如:[{},{}]， 不支持 [1,2,...] 或者 ["x","y"]
数组对象请通过 j2sobject_create_array 创建，传递的 proto 是对象的原型。
只有这样，我们在解析数组的时候才指导如何创建子对象

如果手动创建对象，需要注意必须设置 type/proto 字段以及初始化 next/prev 指针指向自身！

相关使用方法参考： mactbl_array.c

PS. 20240207 添加 j2stable 一个非常简单的基于 json 来实现的数据表，每一个条目我们认为是一个记录一个对象 j2stbl_object
具体实现时需要定义自己的数据格式/表格式（采用结构体表示），结构体在定义时首先引入：J2STBL_DECLARE_OBJECT
例如：
typedef struct {
    J2STBL_DECLARE_OBJECT;
    int id;
    char mac[18];
    int type;
} j2stbl_mactbl_t;

接下来参考j2sobject 使用方法定义：j2sobject_prototype/j2sobject_fields_prototype

需要操作数据时，首先调用 j2stable_init， 第一个参数是要操作的表名称（每个表我们认为时一个 json 文件:name.json）

在 j2stbl_object 对象中有个自增字段： __id__，每次插入（尾部）会自动基于前一条记录 +1；
请注意，该字段只读，请不要操作该字段。

注意，因为这个定位就是处理小数据，在处理较大数据时，并没有考虑太多性能问题！

同时，目前我们使用的 cJSON 不支持流式解析，对于非常大的 json 文件，我们可能也会存在内存性能问题。
关于内存，在 j2stable_init 调用后， json 数据就会被 cache 到内存中，对于数据非常大的情况是不理想的

其他详细使用参考：j2stable_main.c

gcc -g j2sobject.c j2stable.c j2stable_main.c -I. `pkg-config --cflags libcjson` `pkg-config --libs libcjson` -o j2stbl_main


PS. 20240220 在 table 中添加一些表相关辅助字段，例如 version 用于标识版本，如果表结构发生变化，我们可以通过这个字段来进行维护；

这个功能是可以通过宏来控制的 `J2STBL_STORE_METADATA` 如果定义为 `1` ，那么我们的数据将采用如下格式：

```json
{"version":0,"data":[{"__id__":1,"id":10,"mac":"6c:0b:84:3c:71:9e","type":1}]}
```

如果该字段为 `0`， 数据将是下面格式：

```json
[{"__id__":1,"id":10,"mac":"6c:0b:84:3c:71:9e","type":1}]
```
