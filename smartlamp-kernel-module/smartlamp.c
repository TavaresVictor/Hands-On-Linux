#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/usb.h>

MODULE_AUTHOR("Ademar Castro <ademar.castro@icomp.ufam.edu.br>");
MODULE_DESCRIPTION("Driver USB do SmartLamp para ESP32 com CP2102");
MODULE_LICENSE("GPL");

#define MAX_RECV_LINE 100
#define COMMAND_BUFFER_SIZE 64
#define USB_TIMEOUT_MS 2000
#define VENDOR_ID 0x10C4       /* Silicon Labs */
#define PRODUCT_ID 0xEA60      /* CP2102/CP210x */

static struct usb_device *smartlamp_device;
static unsigned int usb_in;
static unsigned int usb_out;
static unsigned int usb_max_size;
static char *usb_in_buffer;
static char *usb_out_buffer;
static struct kobject *sys_obj;
static DEFINE_MUTEX(smartlamp_lock);

static const struct usb_device_id id_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, id_table);

static ssize_t attr_show(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buffer);
static ssize_t attr_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buffer, size_t count);

static struct kobj_attribute led_attribute =
    __ATTR(led, 0644, attr_show, attr_store);
static struct kobj_attribute ldr_attribute =
    __ATTR(ldr, 0444, attr_show, NULL);
static struct attribute *attrs[] = {
    &led_attribute.attr,
    &ldr_attribute.attr,
    NULL,
};
static const struct attribute_group attr_group = {
    .attrs = attrs,
};

static int smartlamp_config_serial(struct usb_device *dev)
{
    int ret;
    __le32 baudrate = cpu_to_le32(9600);

    ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
                          0x00, 0x41, 0x0001, 0,
                          NULL, 0, USB_TIMEOUT_MS);
    if (ret < 0) {
        pr_err("smartlamp: falha ao habilitar UART: %d\n", ret);
        return ret;
    }

    ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
                          0x1E, 0x41, 0, 0,
                          &baudrate, sizeof(baudrate), USB_TIMEOUT_MS);
    if (ret < 0) {
        pr_err("smartlamp: falha ao configurar baud rate: %d\n", ret);
        return ret;
    }

    pr_info("smartlamp: serial configurada em 9600 baud\n");
    return 0;
}

/* Envia "COMANDO\n" ou "COMANDO PARAMETRO\n" ao firmware. */
static int usb_write_serial(const char *cmd, int param)
{
    int ret;
    int actual_size;
    int length;

    if (!smartlamp_device || !usb_out_buffer)
        return -ENODEV;

    if (param < 0)
        length = scnprintf(usb_out_buffer, COMMAND_BUFFER_SIZE,
                           "%s\n", cmd);
    else
        length = scnprintf(usb_out_buffer, COMMAND_BUFFER_SIZE,
                           "%s %d\n", cmd, param);

    ret = usb_bulk_msg(smartlamp_device,
                       usb_sndbulkpipe(smartlamp_device, usb_out),
                       usb_out_buffer, length, &actual_size,
                       USB_TIMEOUT_MS);
    if (ret < 0)
        pr_err("smartlamp: falha ao enviar comando %s: %d\n", cmd, ret);
    else if (actual_size != length)
        return -EIO;

    return ret;
}

/* Recebe linhas, descarta mensagens de outros comandos e retorna o valor. */
static int usb_read_serial(const char *expected_command)
{
    char recv_line[MAX_RECV_LINE];
    char response_command[32];
    int line_length = 0;
    int ret;
    int actual_size;
    int value;
    int i;
    int reads = 0;

    if (!smartlamp_device || !usb_in_buffer)
        return -ENODEV;

    while (reads++ < 10) {
        ret = usb_bulk_msg(smartlamp_device,
                           usb_rcvbulkpipe(smartlamp_device, usb_in),
                           usb_in_buffer, min_t(unsigned int, usb_max_size,
                                                MAX_RECV_LINE - 1),
                           &actual_size, USB_TIMEOUT_MS);
        if (ret < 0)
            return ret;

        for (i = 0; i < actual_size; ++i) {
            char current = usb_in_buffer[i];

            if (current == '\n') {
                recv_line[line_length] = '\0';
                if (sscanf(recv_line, "RES %31s %d", response_command,
                           &value) == 2 &&
                    strcmp(response_command, expected_command) == 0)
                    return value;
                line_length = 0;
                continue;
            }

            if (current == '\r')
                continue;

            if (line_length < MAX_RECV_LINE - 1)
                recv_line[line_length++] = current;
            else
                line_length = 0;
        }
    }

    return -ETIMEDOUT;
}

static int usb_send_cmd(const char *cmd, int param)
{
    int ret;

    ret = usb_write_serial(cmd, param);
    if (ret < 0)
        return ret;

    return usb_read_serial(cmd);
}

static ssize_t attr_show(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buffer)
{
    const char *name = attr->attr.name;
    int value;

    mutex_lock(&smartlamp_lock);
    if (strcmp(name, "led") == 0)
        value = usb_send_cmd("GET_LED", -1);
    else if (strcmp(name, "ldr") == 0)
        value = usb_send_cmd("GET_LDR", -1);
    else
        value = -EINVAL;
    mutex_unlock(&smartlamp_lock);

    if (value < 0)
        return value;
    return scnprintf(buffer, PAGE_SIZE, "%d\n", value);
}

static ssize_t attr_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buffer, size_t count)
{
    const char *name = attr->attr.name;
    long value;
    int ret;

    if (strcmp(name, "led") != 0)
        return -EACCES;

    ret = kstrtol(buffer, 10, &value);
    if (ret < 0 || value < 0 || value > 100)
        return -EINVAL;

    mutex_lock(&smartlamp_lock);
    ret = usb_send_cmd("SET_LED", (int)value);
    mutex_unlock(&smartlamp_lock);

    return ret < 0 ? ret : count;
}

static int usb_probe(struct usb_interface *interface,
                     const struct usb_device_id *id)
{
    struct usb_endpoint_descriptor *endpoint_in;
    struct usb_endpoint_descriptor *endpoint_out;
    int ret;

    ret = usb_find_common_endpoints(interface->cur_altsetting,
                                    &endpoint_in, &endpoint_out,
                                    NULL, NULL);
    if (ret < 0) {
        pr_err("smartlamp: endpoints bulk IN/OUT nao encontrados\n");
        return ret;
    }

    smartlamp_device = usb_get_dev(interface_to_usbdev(interface));
    usb_in = endpoint_in->bEndpointAddress;
    usb_out = endpoint_out->bEndpointAddress;
    usb_max_size = usb_endpoint_maxp(endpoint_in);
    if (usb_max_size == 0) {
        ret = -EINVAL;
        goto error_device;
    }

    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    usb_out_buffer = kmalloc(COMMAND_BUFFER_SIZE, GFP_KERNEL);
    if (!usb_in_buffer || !usb_out_buffer) {
        ret = -ENOMEM;
        goto error_buffers;
    }

    ret = smartlamp_config_serial(smartlamp_device);
    if (ret < 0)
        goto error_buffers;

    sys_obj = kobject_create_and_add("smartlamp", kernel_kobj);
    if (!sys_obj) {
        ret = -ENOMEM;
        goto error_buffers;
    }

    ret = sysfs_create_group(sys_obj, &attr_group);
    if (ret < 0) {
        kobject_put(sys_obj);
        sys_obj = NULL;
        goto error_buffers;
    }

    pr_info("smartlamp: dispositivo conectado; use /sys/kernel/smartlamp/{led,ldr}\n");
    return 0;

error_buffers:
    kfree(usb_in_buffer);
    kfree(usb_out_buffer);
    usb_in_buffer = NULL;
    usb_out_buffer = NULL;
error_device:
    usb_put_dev(smartlamp_device);
    smartlamp_device = NULL;
    return ret;
}

static void usb_disconnect(struct usb_interface *interface)
{
    mutex_lock(&smartlamp_lock);
    if (sys_obj) {
        sysfs_remove_group(sys_obj, &attr_group);
        kobject_put(sys_obj);
        sys_obj = NULL;
    }
    kfree(usb_in_buffer);
    kfree(usb_out_buffer);
    usb_in_buffer = NULL;
    usb_out_buffer = NULL;
    usb_put_dev(smartlamp_device);
    smartlamp_device = NULL;
    mutex_unlock(&smartlamp_lock);

    pr_info("smartlamp: dispositivo desconectado\n");
}

static struct usb_driver smartlamp_driver = {
    .name = "smartlamp",
    .probe = usb_probe,
    .disconnect = usb_disconnect,
    .id_table = id_table,
};

module_usb_driver(smartlamp_driver);
