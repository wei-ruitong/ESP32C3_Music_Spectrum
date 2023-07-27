#include <Arduino.h>
#include <arduinoFFT.h>
#include <Adafruit_NeoPixel.h>
#include <FastLED_NeoMatrix.h>
// 引脚定义
#define MIC_PIN 0
#define WS2812B_PIN 1
#define SCREEN_WIDTH 8
#define SCREEN_HEIGHT 8
// 采样点数量
#define SAMPLES 256
// 采样频率
#define SAMPLINGFREQUENCY 10000 // 意味着声音频率只能采到5khz
// 幅值，512---1000，256---500
#define AMPLITUDE 500
// 噪声,根据情况自行更改
#define NOISE 100
// 采样周期
unsigned int sampling_period_us;
// 定义复数虚部和实部数组
double vReal[SAMPLES];
double vImag[SAMPLES];
// mic模拟变量
unsigned int micvalue;
// 保存先前时间
unsigned long pretime;
// 用来存储每个频段的总幅值
static double bandFrenquency[8] = {0};
// 用来存储上一次每隔频段的峰值
static uint8_t prebandpeak[8] = {0};
// 峰值，用于峰顶下落动画
static uint8_t peak_temp[8] = {0};
unsigned char ledid;
// 创建FFT对象
arduinoFFT fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLINGFREQUENCY);
Adafruit_NeoPixel pixels(SCREEN_WIDTH *SCREEN_HEIGHT, WS2812B_PIN, NEO_GRB + NEO_KHZ800);
// 函数声明
void get_band_peak(double *bandFrenquency);
void drawBandwithoutpeak(int band, int bandheight);
void drawBandpeak(int band);
// 定义8种颜色
uint32_t color_list[8] = {0xff0000, 0xff8000, 0xffff00, 0x00c957,
                          0x3d9140, 0x87ceeb, 0x0000ff, 0xa020f0};
void setup()
{
  Serial.begin(115200);
  pixels.setBrightness(100);
  // 获取采样周期，也就是多少秒采集一次数据
  sampling_period_us = round(1000000 * (1.0 / SAMPLINGFREQUENCY));
}
void loop()
{
  pixels.clear();
  get_band_peak(bandFrenquency);
  pixels.show();
}
void get_band_peak(double *bandFrenquency)
{
  for (int i = 0; i < 8; i++)
  {
    bandFrenquency[i] = 0;
  }
  // ADC采样
  for (int i = 0; i < SAMPLES; i++)
  {
    pretime = micros();
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0.0;
    // 如果ADC转化时间大于采样时间，就进行下一次采样，否则就等待
    while (micros() < (pretime + sampling_period_us))
      ;
  }
  fft.DCRemoval();
  // 窗函数，降低弥散影响
  fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  // 计算fft
  fft.Compute(FFT_FORWARD);
  // 使用虚值和实值计算幅值,幅值会存储到vReal数组中
  fft.ComplexToMagnitude();
  // fft.MajorPeak(vReal, SAMPLES, SAMPLINGFREQUENCY);
  // 进行频率划分，划分策略
  // 8 bands, 5kHz top band
  for (int i = 2; i < SAMPLES / 2; i++)
  {
    if (vReal[i] > NOISE)
    {
      // SAMPLES = 512 ,此时对于esp32c3 速度还算可以
      // 如果值大于512，一般为2的n次幂，FFT比较耗时，UI看起来不是很流畅
#if 0
      if (i<=19 )           bandFrenquency[0]  += (int)vReal[i];
      if (i>19   && i<=29  ) bandFrenquency[1]  += (int)vReal[i];
      if (i>29   && i<=43  ) bandFrenquency[2]  += (int)vReal[i];
      if (i>43   && i<=64  ) bandFrenquency[3]  += (int)vReal[i];
      if (i>64   && i<=96  ) bandFrenquency[4]  += (int)vReal[i];
      if (i>96   && i<=143  ) bandFrenquency[5]  += (int)vReal[i];
      if (i>143   && i<=214  ) bandFrenquency[6]  += (int)vReal[i];
      if (i>214             ) bandFrenquency[7]  += (int)vReal[i];
#endif

#if 1
      // SAMPLES = 256
      if (i <= 10)
        bandFrenquency[0] += (int)vReal[i];
      if (i > 10 && i <= 14)
        bandFrenquency[1] += (int)vReal[i];
      if (i > 14 && i <= 21)
        bandFrenquency[2] += (int)vReal[i];
      if (i > 21 && i <= 32)
        bandFrenquency[3] += (int)vReal[i];
      if (i > 32 && i <= 48)
        bandFrenquency[4] += (int)vReal[i];
      if (i > 48 && i <= 71)
        bandFrenquency[5] += (int)vReal[i];
      if (i > 71 && i <= 107)
        bandFrenquency[6] += (int)vReal[i];
      if (i > 107)
        bandFrenquency[7] += (int)vReal[i];
#endif
    }
  }
  for (uint8_t i = 0; i < 8; i++)
  {
    // 获取每隔频段的幅值，并缩放
    int bandheight = bandFrenquency[i] / AMPLITUDE;
    // 因为第一个band的振幅比较大，这里进行单独的缩放
    if (i == 0)
    {
      bandheight = bandheight / 2;
      if (bandheight >= 1)
        bandheight -= 1;
    }
    if (i == 0 && bandheight >= 2)
      bandheight -= 2;
    // 注意这里必须是SCREEN_HEIGHT，否则bandheight永远达不到顶峰
    if (bandheight > SCREEN_HEIGHT)
      bandheight = SCREEN_HEIGHT;
    // 平均值，使更加平滑
    bandheight = (prebandpeak[i] + bandheight) / 2;
    // 使峰顶跳动起来，需要保存两次峰值之间的最大值
    if (bandheight > peak_temp[i])
    {
      // 更新peak值,只有大于上次的峰值才会更新峰值
      peak_temp[i] = min(SCREEN_HEIGHT, bandheight);
    }
    // 绘制band,但不绘制band的顶部
    drawBandwithoutpeak(i, bandheight);
    // 绘制每条band的顶部
    drawBandpeak(i);
    prebandpeak[i] = bandheight;
  }
  // 下落动画
  EVERY_N_MILLISECONDS(120)
  {
    for (uint8_t i = 0; i < SCREEN_WIDTH; i++)
      if (peak_temp[i] > 0)
        peak_temp[i] -= 1;
  }
}
void drawBandwithoutpeak(int band, int bandheight)
{
  // 绘制每一个条带，不包含每个条带的峰值
  if (bandheight != 0)
  {
    // 蛇形走位，需要判断是奇数列，还是偶数列
    if (band % 2)
    {
      // 奇数列1、3、5、7
      for (uint8_t i = 0; i < bandheight; i++)
      {
        // 计算每一个LED对应的顺序号（0~63）
        ledid = (band + 1) * SCREEN_HEIGHT - i - 1;
        pixels.setPixelColor(ledid, color_list[band]);
      }
    }
    else
    {
      // 偶数列0、2、4、6
      for (uint8_t i = 0; i < bandheight; i++)
      {
        // 计算每一个LED对应的顺序号（0~63）
        ledid = band * SCREEN_HEIGHT + i;
        pixels.setPixelColor(ledid, color_list[band]);
      }
    }
  }
}
void drawBandpeak(int band)
{
  // 计算每个峰顶LED的编号
  if (band % 2)
  {
    // 奇数列1、3、5、7
    ledid = (band + 1) * SCREEN_HEIGHT - peak_temp[band] - 1;
  }
  else
  {
    // 偶数列0、2、4、6
    ledid = band * SCREEN_HEIGHT + peak_temp[band];
  }
  pixels.setPixelColor(ledid, 0xffffff);
}
