#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>

/* ==================== Config ==================== */
#define UART_NAME       "uart2"
#define ESP32_UART      "uart3"
#define CMD_MAX_LEN     20

/* PWM设备名称 */
#define PWM2_NAME       "pwm2"
#define PWM9_NAME       "pwm9"
#define PWM10_NAME      "pwm10"
#define PWM14_NAME      "pwm14"

/* SU03T Hex Command Definitions */
#define CMD_WAKE        0x57,0x41,0x4B,0x45
#define CMD_STOP        0x53,0x54,0x4F,0x50
#define CMD_LOWER       0x4C,0x4F,0x57,0x45,0x52
#define CMD_FRONT       0x46,0x52,0x4F,0x4E,0x54
#define CMD_BACK        0x42,0x41,0x43,0x4B
#define CMD_LEFT        0x4C,0x45,0x46,0x54
#define CMD_RIGHT       0x52,0x49,0x47,0x48,0x54
#define CMD_SIT         0x53,0x49,0x54

/* ==================== Servo Config ==================== */
#define SERVO_COUNT     5

typedef struct {
    const char *pwm_name;
    int channel;
    const char *name;
    rt_uint32_t center;
    rt_uint32_t min;
    rt_uint32_t max;
} servo_config_t;

static servo_config_t servos[SERVO_COUNT] = {
    {PWM14_NAME, 1, "FrontLeft",  1500000, 1000000, 2000000},
    {PWM9_NAME,  1, "FrontRight", 1500000, 1000000, 2000000},
    {PWM2_NAME,  2, "RearLeft",   1500000, 1000000, 2000000},
    {PWM10_NAME, 1, "RearRight",  1500000, 1000000, 2000000},
    {PWM2_NAME,  1, "Tail",       1500000, 1000000, 2000000},
};

static struct rt_device_pwm *servo_pwm_dev[SERVO_COUNT] = {RT_NULL};

/* ==================== UART Variables ==================== */
static rt_device_t uart_dev = RT_NULL;
static rt_device_t esp32_dev = RT_NULL;
static uint8_t cmd_buf[CMD_MAX_LEN + 1];
static uint8_t cmd_len = 0;
static uint8_t cmd_ready = 0;
static rt_tick_t last_recv_tick = 0;

/* ==================== Servo Functions ==================== */
void servo_set_pulse(int id, rt_uint32_t pulse_ns)
{
    if (id < 0 || id >= SERVO_COUNT || !servo_pwm_dev[id])
        return;

    if (pulse_ns < servos[id].min) pulse_ns = servos[id].min;
    if (pulse_ns > servos[id].max) pulse_ns = servos[id].max;

    rt_pwm_set(servo_pwm_dev[id], servos[id].channel, 20000000, pulse_ns);
}

void servo_set_angle(int id, int angle)
{
    if (id < 0 || id >= SERVO_COUNT)
        return;

    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;

    rt_uint32_t range = servos[id].max - servos[id].min;
    rt_uint32_t pulse = servos[id].min + (range * angle / 180);

    servo_set_pulse(id, pulse);
}

void servo_init(void)
{
    rt_kprintf("=== Servo Init ===\n");

    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_pwm_dev[i] = (struct rt_device_pwm *)rt_device_find(servos[i].pwm_name);
        if (!servo_pwm_dev[i])
        {
            rt_kprintf("ERROR: %s not found for %s!\n", servos[i].pwm_name, servos[i].name);
            continue;
        }

        rt_pwm_enable(servo_pwm_dev[i], servos[i].channel);
        rt_pwm_set(servo_pwm_dev[i], servos[i].channel, 20000000, servos[i].center);
        rt_kprintf("[%s] %s CH%d OK\n", servos[i].pwm_name, servos[i].name, servos[i].channel);
    }

    rt_kprintf("All servos centered\n");
}

void servo_center(void)
{
    for (int i = 0; i < SERVO_COUNT; i++)
        servo_set_pulse(i, servos[i].center);
}

/* ==================== Action Functions ==================== */
void action_front(void)
{
    rt_kprintf(">>> Action: Walk Forward\n");

    for (int step = 0; step < 2; step++)
    {
        // ===== 第1步：左前+右后 向前迈20度 =====
        rt_kprintf("    Step %d: FrontLeft+RearRight forward\n", step + 1);
        servo_set_angle(0, 70);   // 左前腿：90° - 20° = 70° (向前)
        servo_set_angle(3, 70);   // 右后腿：90° - 20° = 70° (向前)
        rt_thread_mdelay(400);    // 停一下，让动作到位

        // ===== 回正 =====
        servo_set_angle(0, 90);   // 左前腿回正
        servo_set_angle(3, 90);   // 右后腿回正
        rt_thread_mdelay(400);    // 停一下

        // ===== 第2步：右前+左后 向前迈20度 =====
        rt_kprintf("    Step %d: FrontRight+RearLeft forward\n", step + 1);
        servo_set_angle(1, 70);   // 右前腿：90° - 20° = 70° (向前)
        servo_set_angle(2, 70);   // 左后腿：90° - 20° = 70° (向前)
        rt_thread_mdelay(400);    // 停一下

        // ===== 回正 =====
        servo_set_angle(1, 90);   // 右前腿回正
        servo_set_angle(2, 90);   // 左后腿回正
        rt_thread_mdelay(400);    // 停一下
    }

    rt_kprintf(">>> Walk Forward complete\n");
}

void action_back(void)
{
    rt_kprintf(">>> Action: Walk Backward\n");

    for (int step = 0; step < 2; step++)
    {
        servo_set_angle(0, 130);
        servo_set_angle(3, 50);
        servo_set_angle(1, 80);
        servo_set_angle(2, 100);
        rt_thread_mdelay(350);

        servo_set_angle(0, 90);
        servo_set_angle(1, 90);
        servo_set_angle(2, 90);
        servo_set_angle(3, 90);
        rt_thread_mdelay(200);

        servo_set_angle(1, 130);
        servo_set_angle(2, 50);
        servo_set_angle(0, 100);
        servo_set_angle(3, 80);
        rt_thread_mdelay(350);

        servo_center();
        rt_thread_mdelay(200);
    }
}

void action_left(void)
{
    rt_kprintf(">>> Action: Turn Left\n");

    servo_set_angle(0, 135);
    servo_set_angle(2, 135);
    servo_set_angle(1, 45);
    servo_set_angle(3, 45);
    rt_thread_mdelay(500);

    servo_set_angle(0, 110);
    servo_set_angle(1, 70);
    servo_set_angle(2, 70);
    servo_set_angle(3, 110);
    rt_thread_mdelay(300);

    servo_set_angle(0, 130);
    servo_set_angle(2, 130);
    servo_set_angle(1, 50);
    servo_set_angle(3, 50);
    rt_thread_mdelay(500);

    servo_set_angle(0, 100);
    servo_set_angle(1, 80);
    servo_set_angle(2, 80);
    servo_set_angle(3, 100);
    rt_thread_mdelay(300);

    servo_center();
}

void action_right(void)
{
    rt_kprintf(">>> Action: Turn Right\n");

    servo_set_angle(1, 135);
    servo_set_angle(3, 135);
    servo_set_angle(0, 45);
    servo_set_angle(2, 45);
    rt_thread_mdelay(500);

    servo_set_angle(0, 70);
    servo_set_angle(1, 110);
    servo_set_angle(2, 110);
    servo_set_angle(3, 70);
    rt_thread_mdelay(300);

    servo_set_angle(1, 130);
    servo_set_angle(3, 130);
    servo_set_angle(0, 50);
    servo_set_angle(2, 50);
    rt_thread_mdelay(500);

    servo_set_angle(0, 80);
    servo_set_angle(1, 100);
    servo_set_angle(2, 100);
    servo_set_angle(3, 80);
    rt_thread_mdelay(300);

    servo_center();
}

void action_sit(void)
{
    rt_kprintf(">>> Action: Sit Down\n");

    // ===== 阶段1：后腿开始往前收，前腿微调保持平衡 =====
    servo_set_angle(2, 50);   // 左后腿往前收（90→50，向前40度）
    servo_set_angle(3, 130);  // 右后腿往前收（90→130，向前40度）
    rt_thread_mdelay(400);

    // ===== 阶段2：后腿继续往前往上收 =====
    servo_set_angle(2, 30);   // 左后腿收到最前
    servo_set_angle(3, 150);  // 右后腿收到最前
    rt_thread_mdelay(400);

    // ===== 阶段3：保持坐姿，尾巴摇摆 =====
    rt_thread_mdelay(300);

    // 尾巴摇一摇
    servo_set_pulse(4, 1200000);  // 尾巴左摆
    rt_thread_mdelay(150);
    servo_set_pulse(4, 1800000);  // 尾巴右摆
    rt_thread_mdelay(150);
    servo_set_pulse(4, 1500000);  // 尾巴回中

    rt_kprintf(">>> Sit complete\n");
}

void action_lower(void)
{
    rt_kprintf(">>> Action: Lie Down\n");

    servo_set_angle(0, 130);
    servo_set_angle(1, 50);
    servo_set_angle(2, 130);
    servo_set_angle(3, 50);
    rt_thread_mdelay(400);

    servo_set_angle(0, 150);
    servo_set_angle(1, 30);
    servo_set_angle(2, 150);
    servo_set_angle(3, 30);
    rt_thread_mdelay(400);

    servo_set_angle(0, 160);
    servo_set_angle(1, 20);
    servo_set_angle(2, 160);
    servo_set_angle(3, 20);
    rt_thread_mdelay(300);
}

void action_wake(void)
{
    rt_kprintf(">>> Action: Wake Up\n");

    // 尾巴来回摆动两次
    for (int i = 0; i < 2; i++)
    {
        servo_set_pulse(4, 1100000);  // 尾巴摆到左边
        rt_thread_mdelay(150);
        servo_set_pulse(4, 1900000);  // 尾巴摆到右边
        rt_thread_mdelay(150);
    }

    // 尾巴回中
    servo_set_pulse(4, 1500000);

    rt_kprintf(">>> Wake complete\n");
}
/* ==================== ESP32 Communication ==================== */
void esp32_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    rt_kprintf("=== ESP32 UART Init ===\n");

    esp32_dev = rt_device_find(ESP32_UART);
    if (esp32_dev == RT_NULL)
    {
        rt_kprintf("ERROR: ESP32 %s not found!\n", ESP32_UART);
        return;
    }

    config.baud_rate = BAUD_RATE_115200;
    rt_device_control(esp32_dev, RT_DEVICE_CTRL_CONFIG, &config);

    if (rt_device_open(esp32_dev, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("ERROR: ESP32 %s open failed!\n", ESP32_UART);
        esp32_dev = RT_NULL;
        return;
    }

    rt_kprintf("ESP32 %s opened OK, Baud:115200\n", ESP32_UART);
}

void esp32_send_cmd(const char *cmd)
{
    if (esp32_dev == RT_NULL) return;

    int len = rt_strlen(cmd);
    rt_device_write(esp32_dev, 0, cmd, len);
    rt_kprintf("-> ESP32: [%s]\n", cmd);
}

/* ==================== Command Parser ==================== */
void cmd_process(void)
{
    rt_kprintf(">>> Command received: ");
    for (int i = 0; i < cmd_len; i++)
        rt_kprintf("%02X ", cmd_buf[i]);
    rt_kprintf("\n");

    uint8_t wake[]  = {CMD_WAKE};
    uint8_t stop[]  = {CMD_STOP};
    uint8_t lower[] = {CMD_LOWER};
    uint8_t front[] = {CMD_FRONT};
    uint8_t back[]  = {CMD_BACK};
    uint8_t left[]  = {CMD_LEFT};
    uint8_t right[] = {CMD_RIGHT};
    uint8_t sit[]   = {CMD_SIT};

    if (cmd_len == sizeof(wake) && memcmp(cmd_buf, wake, sizeof(wake)) == 0)
    {
        rt_kprintf(">>> Matched: WAKE -> ESP32\n");
        esp32_send_cmd("WAKE\n");
        action_wake();
    }
    else if (cmd_len == sizeof(stop) && memcmp(cmd_buf, stop, sizeof(stop)) == 0)
    {
        rt_kprintf(">>> Matched: STOP -> ESP32\n");
        esp32_send_cmd("STOP\n");
    }
    else if (cmd_len == sizeof(lower) && memcmp(cmd_buf, lower, sizeof(lower)) == 0)
        action_lower();
    else if (cmd_len == sizeof(front) && memcmp(cmd_buf, front, sizeof(front)) == 0)
        action_front();
    else if (cmd_len == sizeof(back) && memcmp(cmd_buf, back, sizeof(back)) == 0)
        action_back();
    else if (cmd_len == sizeof(left) && memcmp(cmd_buf, left, sizeof(left)) == 0)
        action_left();
    else if (cmd_len == sizeof(right) && memcmp(cmd_buf, right, sizeof(right)) == 0)
        action_right();
    else if (cmd_len == sizeof(sit) && memcmp(cmd_buf, sit, sizeof(sit)) == 0)
        action_sit();
    else
        rt_kprintf(">>> Unknown command\n");
}

/* ==================== UART Init ==================== */
void su03t_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    uart_dev = rt_device_find(UART_NAME);
    if (uart_dev == RT_NULL)
    {
        rt_kprintf("ERROR: %s not found!\n", UART_NAME);
        return;
    }

    config.baud_rate = BAUD_RATE_9600;
    rt_device_control(uart_dev, RT_DEVICE_CTRL_CONFIG, &config);

    if (rt_device_open(uart_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("ERROR: %s open failed!\n", UART_NAME);
        return;
    }

    rt_kprintf("SU03T UART2 Init OK, Baud:9600\n");
}

/* ==================== Receive Thread ==================== */
void su03t_recv_thread(void *param)
{
    char rx_byte;

    while (1)
    {
        while (rt_device_read(uart_dev, 0, &rx_byte, 1) == 1)
        {
            if (rx_byte == 0x00)
            {
                if (cmd_len > 0)
                    cmd_ready = 1;
            }
            else if (cmd_len < CMD_MAX_LEN)
            {
                cmd_buf[cmd_len++] = rx_byte;
                last_recv_tick = rt_tick_get();
            }
        }

        if (cmd_len > 0 && cmd_ready == 0 &&
            (rt_tick_get() - last_recv_tick) > rt_tick_from_millisecond(50))
        {
            cmd_ready = 1;
        }

        rt_thread_mdelay(5);
    }
}

/* ==================== Main ==================== */
int main(void)
{
    rt_thread_t tid;

    rt_kprintf("\n========== Robot Dog Start ==========\n");

    servo_init();
    su03t_init();
    esp32_init();

    tid = rt_thread_create("su03t_rx", su03t_recv_thread, RT_NULL, 1024, 10, 5);
    if (tid)
    {
        rt_thread_startup(tid);
        rt_kprintf("Receive thread created OK\n");
    }

    rt_kprintf("Waiting for voice commands...\n");

    while (1)
    {
        if (cmd_ready)
        {
            cmd_ready = 0;
            cmd_process();
            rt_memset(cmd_buf, 0, sizeof(cmd_buf));
            cmd_len = 0;
        }
        rt_thread_mdelay(10);
    }

    return 0;
}
