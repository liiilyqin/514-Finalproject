// 定义步进电机控制引脚
#define COIL_A1 D0
#define COIL_A2 D1
#define COIL_B1 D2
#define COIL_B2 D3

// 定义步进电机步进序列（全步模式）
const int stepSequence[4][4] = {
    {1, 0, 1, 0}, // Step 1
    {0, 1, 1, 0}, // Step 2
    {0, 1, 0, 1}, // Step 3
    {1, 0, 0, 1}  // Step 4
};

// 步进电机角度转换（200步 = 360°）
#define STEPS_PER_REV 200
#define DEGREE_TO_STEP(deg) ((deg) * STEPS_PER_REV / 360)

// 定义分贝范围对应的角度
#define NORMAL_ANGLE 0
#define HIGHER_ANGLE 90
#define DANGEROUS_ANGLE 180

// 当前步进电机角度
int currentAngle = 0;

void setup() {
    Serial.begin(9600); // 初始化串口通信，波特率9600
    pinMode(COIL_A1, OUTPUT);
    pinMode(COIL_A2, OUTPUT);
    pinMode(COIL_B1, OUTPUT);
    pinMode(COIL_B2, OUTPUT);
}

// 步进电机旋转到指定角度
void rotateToAngle(int targetAngle, int stepDelay) {
    int targetStep = DEGREE_TO_STEP(targetAngle);
    int currentStep = DEGREE_TO_STEP(currentAngle);
    int stepDiff = targetStep - currentStep;
    
    if (stepDiff > 0) {
        stepMotor(stepDiff, false, stepDelay);  // 正向旋转
    } else if (stepDiff < 0) {
        stepMotor(-stepDiff, true, stepDelay); // 反向旋转
    }
    
    currentAngle = targetAngle;  // 更新当前角度
}

// 步进电机步进控制
void stepMotor(int steps, bool reverse, int stepDelay) {
    for (int i = 0; i < steps; i++) {
        for (int step = 0; step < 4; step++) {
            int s = reverse ? (3 - step) : step;
            digitalWrite(COIL_A1, stepSequence[s][0]);
            digitalWrite(COIL_A2, stepSequence[s][1]);
            digitalWrite(COIL_B1, stepSequence[s][2]);
            digitalWrite(COIL_B2, stepSequence[s][3]);
            delay(stepDelay);
        }
    }
}

// 定义分贝数组
int decibel[] = {30, 60, 90, 45, 60, 98, 30};
int decibelIndex = 0; // 记录当前索引

void loop() {
    int db = decibel[decibelIndex]; // 获取当前分贝值

    // 判断分贝范围并调整步进电机角度
    if (db <= 50) {
        rotateToAngle(NORMAL_ANGLE, 10);
    } else if (db > 50 && db <= 70) {
        rotateToAngle(HIGHER_ANGLE, 10);
    } else {
        rotateToAngle(DANGEROUS_ANGLE, 10);
    }

    // 串口输出分贝值
    Serial.print("Decibel: ");
    Serial.print(db);
    Serial.println(" dB");

    // 更新索引，循环遍历数组
    decibelIndex++;
    if (decibelIndex >= sizeof(decibel) / sizeof(decibel[0])) {
        decibelIndex = 0; // 重新回到第一个数据
    }

    delay(2000); // 每2秒更新一次
}
