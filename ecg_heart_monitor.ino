#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>

//the MKRzero has 32 kB of SRAM!
#define ECG_pin   A1
#define select_button_pin 1
#define option_button_pin 2
#define num_options 8;
#define num_log_modes 2;
#define event_log_interval   10000 //us
#define data_plot_interval   20000 //us
#define pulse_meas_debounce 100000 //us
#define plot_data_length   128
#define event_data_length   2048 //8 kB each, for 16 kB of event log space between the timer and event data
#define signal_avg_frac_new_baseline   0.1
#define signal_avg_frac_new_peak   0.05

// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_MOSI   8
#define OLED_CLK   9
#define OLED_DC    12
#define OLED_CS    11
#define OLED_RESET 0
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

#define heart_HEIGHT   8
#define heart_WIDTH    9
static const unsigned char PROGMEM heart_bmp[] = {
  B01100011, B01111011,
  B11111111, B11111111,
  B11110111, B11110001,
  B11110000, B01110000,
  B00010000
};

File dataFile;
unsigned long timer = 0;
unsigned long data_collected_last_time = 0;
unsigned long event_collected_last_time = 0;
uint16_t plot_data[plot_data_length] = {};
uint8_t plot_index = 0;
unsigned long plot_updated_time = 0;
unsigned long event_time[event_data_length] = {};
double event_data[event_data_length] = {};
uint16_t event_index = 0;
uint8_t heart_rate = 60;
boolean new_pulse = false;
uint8_t log_mode = 0;
double pulse_ECG_thresh = 0.6;//mV
double baseline_signal = 0;
double above_thresh_avg = 5.00;
double pulse_delay = 100000;
double batt_avg = 0;
unsigned long pulse_timer = 0;
unsigned long flip_debounce = 0;
boolean select_button_state = false;
boolean option_button_state = false;
boolean screen_state = true;
boolean logging_state = false;
boolean above_thresh = false;
boolean re_scale_plot = true;
boolean flip_data = false;
boolean autoflip_ECG = false;
uint8_t option_mode = 0;
uint8_t option_state = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Clear the buffer to get rid of the Adafruit logo
  display.clearDisplay();

  analogReadResolution(12);//12-bit AtoD resolution

  //initialize our array
  for (int i = 0; i < plot_data_length; i++)
  {
    plot_data[i] = 0;
  }

  timer = micros();
}


void loop() {
  uint16_t AtoD = analogRead(ECG_pin);
  double ECG_data = AtoD * (3.300 / 4095.000) * 1000.000 / 328.1018; //mV
  timer = micros();

  /*
    Serial.print((above_thresh + baseline_signal) * 1000);
    Serial.print(",");
    Serial.print((above_thresh_avg + baseline_signal) * 1000);
    Serial.print(",");
    Serial.print(baseline_signal * 1000);
    Serial.print(",");
    Serial.print((pulse_ECG_thresh + baseline_signal) * 1000);
    Serial.print(",");
    Serial.println(ECG_data * 1000); //output the data that we can visualize, mV units
  */
  //extract the heart rate
  double diff_ECG = ECG_data - baseline_signal;
  double abs_diff_ECG = abs(diff_ECG);
  if (timer - pulse_timer > 0.8 * pulse_delay) //delay before next detect varies with pulse rate
  {
    if (abs_diff_ECG > pulse_ECG_thresh && !above_thresh)
    {
      //above pulse detect threshold, timer expired and last measurement was below the threshold -> we've caught the rising edge
      above_thresh_avg = above_thresh_avg * (1.00 - signal_avg_frac_new_peak) + signal_avg_frac_new_peak * diff_ECG;//we take an average of points where

      pulse_delay = (timer - pulse_timer);
      heart_rate = 60000000.00 / pulse_delay;
      new_pulse = true;
      pulse_timer = timer;
      above_thresh = true;
      if (pulse_delay > 500000)
      {
        pulse_delay = 500000;//30 bpm minimum
      }
      if (timer - flip_debounce > 3000000 && autoflip_ECG)
      {
        if (above_thresh_avg < 0.0 && !flip_data)//let's try to keep the QRS wave upright - this might not work for bad thresholding or strange waveforms
        {
          flip_data = true;
          flip_debounce = timer;
        }
        else if (above_thresh_avg >= 0.0 && flip_data)
        {
          flip_data = false;
          flip_debounce = timer;
        }
      }

    }
    else
    {
      //below threshold, timer expired
      //adjust the baseline
      baseline_signal = baseline_signal * (1.00 - signal_avg_frac_new_baseline) + signal_avg_frac_new_baseline * ECG_data;
      above_thresh = false;
    }
  }


  //log data if appropriate
  if (logging_state)
  {
    // open the file. If we do this for each write, we slow the whole process
    //File dataFile = SD.open("datalog.txt", FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      if (log_mode == 0)
      {
        //log full ECG
        dataFile.print(timer);
        dataFile.print(F(","));
        dataFile.print(ECG_data, 4);//mV, steps of 2.45 microvolts per AtoD increment
        dataFile.print(F(","));
        if (new_pulse)
        {
          dataFile.print(heart_rate);
          new_pulse = false;
        }
        else
        {
          dataFile.print(F("0"));
        }
        dataFile.print(F(","));
        dataFile.println();
        //dataFile.close();//If we do this for each write, we slow the whole process
      }
      else if (log_mode == 1 && new_pulse)
      {
        //log pulse only, and only at changed values
        dataFile.print(timer);
        dataFile.print(F(",0,"));
        dataFile.print(heart_rate);
        dataFile.print(F(","));
        dataFile.println();
        new_pulse = false;
      }
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println(F("error opening datalog.txt"));
    }
  }

  //periodically collect analog data to plot
  if ((timer - data_collected_last_time) >= data_plot_interval)
  {
    if (flip_data)
    {
      AtoD = 4095 - AtoD;
    }
    plot_index = update_data_array(AtoD, plot_data, plot_data_length, plot_index);
    data_collected_last_time = timer;
  }

  if ((timer - event_collected_last_time) >= event_log_interval)
  {
    //also collect data for the event log
    event_time[event_index] = timer;
    event_data[event_index] = ECG_data;
    event_index = (event_index + 1) % event_data_length;
    event_collected_last_time = timer;
  }

  //evaluate button presses to change states that inform UI actions
  if (!select_button_state && digitalRead(select_button_pin))
  {
    select_button_state = true;
    if (option_mode == 0)
    {
      //screen on or off
      screen_state = !screen_state;
    }
    else if (option_mode == 1)
    {
      //adjust logging
      logging_state = !logging_state;
      if (logging_state) //if we just began logging, write the data header to demark the beginning of new data
      {
        if (!SD.begin(SDCARD_SS_PIN)) {
          Serial.println(F("Card failed, or not present"));
        }
        Serial.println(F("card initialized."));
        print_log_header();
        // open the file.
        dataFile = SD.open("datalog.txt", FILE_WRITE);
      }
      else
      {
        //close the file, finalizing data save
        dataFile.close();
      }
    }
    else if (option_mode == 2)
    {
      //manual pulse threshold adjust
      pulse_ECG_thresh += 0.1;
      if (pulse_ECG_thresh > 1.6)
      {
        pulse_ECG_thresh = 0.1;
      }
    }
    else if (option_mode == 3)
    {
      log_mode = (log_mode + 1) % num_log_modes;
    }
    else if (option_mode == 4)
    {
      re_scale_plot = !re_scale_plot;
    }
    else if (option_mode == 5)
    {
      //collect the event data!
      logging_state = !logging_state;
      if (logging_state) //if we just began logging, write the data header to demark the beginning of new data
      {
        if (!SD.begin(SDCARD_SS_PIN)) {
          Serial.println(F("Card failed, or not present"));
        }
        Serial.println(F("card initialized."));
        print_log_header();
        // open the file.
        dataFile = SD.open("datalog.txt", FILE_WRITE);
        //write the event log
        for (uint16_t i = 0; i < event_data_length; i++)
        {
          uint16_t print_event_index = (event_index + i) % event_data_length;
          dataFile.print(event_time[print_event_index]);
          dataFile.print(F(","));
          dataFile.print(event_data[print_event_index], 4);//mV, steps of 2.45 microvolts per AtoD increment
          dataFile.println(F(",0,"));
        }
      }
      else
      {
        //close the file, finalizing data save
        dataFile.close();
      }
    }
    else if (option_mode == 6)
    {
      //turn off auto flipping
      autoflip_ECG = !autoflip_ECG;
    }
    else if (option_mode == 7)
    {
      //flip the ECG data plotting
      flip_data = !flip_data;
    }
  }
  else if (select_button_state && !digitalRead(select_button_pin))
  {
    select_button_state = false;
  }

  if (screen_state && !option_button_state && digitalRead(option_button_pin))
  {
    option_button_state = true;
    option_mode += 1;
    option_mode = option_mode % num_options;
  }
  else if (screen_state && option_button_state && !digitalRead(option_button_pin))
  {
    option_button_state = false;
  }

  //finally, periodically update the screen
  if ((timer - plot_updated_time) >= 33333)//33333
  {
    //30 hz screen update
    display.clearDisplay();

    if (screen_state) {
      //plot the analog input data
      plot_array(0, 10, plot_data_length, 53, plot_data, plot_data_length, plot_index, re_scale_plot, true);

      //show heart rate data
      display_heart_rate(1, 1);

      //monitor battery periodically too
      display_batt_status(102, 1);

      //then, depending on the state, show an option to select
      display.setTextSize(1);
      if (option_mode == 1)
      {
        display.setCursor(30, 16);
        if (logging_state)
        {
          display.print(F("log active"));
        }
        else if (!logging_state)
        {
          display.print(F("log inactive"));
        }
      }
      else if (option_mode == 2)
      {
        display.setCursor(10, 16);
        display.print(F("pulse thresh: "));
        display.print(pulse_ECG_thresh);
      }
      else if (option_mode == 3)
      {
        display.setCursor(10, 16);
        display.print(F("Log mode: "));
        if (log_mode == 0)
        {
          display.print(F("ECG"));
        }
        else if (log_mode == 1)
        {
          display.print(F("Pulse"));
        }
      }
      else if (option_mode == 4)
      {
        display.setCursor(10, 16);
        display.print(F("Rescale: "));
        if (re_scale_plot)
        {
          display.print(F("True"));
        }
        else
        {
          display.print(F("False"));
        }
      }
      else if (option_mode == 5)
      {
        display.setCursor(10, 16);
        if (logging_state)
        {
          display.print(F("Event: log active"));
        }
        else if (!logging_state)
        {
          display.print(F("Event: log inactive"));
        }
      }
      else if (option_mode == 6)
      {
        display.setCursor(10, 16);
        if (autoflip_ECG)
        {
          display.print(F("Autoflip: active"));
        }
        else if (!autoflip_ECG)
        {
          display.print(F("Autoflip: inactive"));
        }
      }
      else if (option_mode == 7)
      {
        display.setCursor(10, 16);
        display.print(F("Flip ECG"));
      }
    }

    //and finally update the display output
    display.display();
    plot_updated_time = timer;
  }

}

//use a very dense left-to-right binary array of data
//to signify a bitmap image to load
void load_bitmap( int x_loc, int y_loc, const unsigned char data[], int width, int height)
{
  for (int i = 0; i < width * height; i++)
  {
    uint8_t mask = 0x80 >> (i % 8);
    if ((data[i / 8] & mask) != 0)
    {
      uint8_t width_offset = i % width;
      uint8_t height_offset = i / width;
      display.drawPixel(x_loc + width_offset, y_loc + height_offset, WHITE);
    }
  }
}

void findMax(uint16_t data[], uint8_t data_length, double &data_max, double &data_min)
{
  data_max = 0;
  data_min = 4096;
  for (int i = 0; i < data_length; i++)
  {
    if (data[i] < data_min)
    {
      data_min = data[i];
    }
    if (data[i] > data_max)
    {
      data_max = data[i];
    }
  }
}

void plot_array(int x_loc, int y_loc, int width, int height, uint16_t data[], uint8_t data_length, uint8_t start_index, boolean rescale, boolean vertical_fill)
{

  double data_max = 4096.00;
  double data_min = 0.00;
  if (rescale)
  {
    findMax(data, data_length, data_max, data_min);
  }
  double data_y_scaler = height / (data_max - data_min);

  //for x-data length, we need to decide if we need to skip data in order
  //to fully plot it, or if we need to interpolate to fill in space
  //better yet, i'll ignore that for now and only plot 'width' amount of data
  int plot_width = width;
  if (data_length < width)
  {
    plot_width = data_length;
  }

  int prev_height = 0;
  for (int i = 0; i < plot_width; i++)
  {
    uint8_t index = (i + start_index) % data_length;
    int data_height = height - (data[index] - data_min) * data_y_scaler;
    if (vertical_fill && i > 0)
    {
      int data_step = prev_height - data_height;
      int abs_data_step = abs(data_step);

      if (abs_data_step > 1)
      {
        int direc = -1;
        if (data_step < 0)
        {
          direc = 1;
        }
        for (int j = 0; j < abs_data_step; j++)
        {
          display.drawPixel(x_loc + i, y_loc + prev_height + direc * j, WHITE);
        }
      }
    }
    prev_height = data_height;
    display.drawPixel(x_loc + i, y_loc + data_height, WHITE);
  }
}

uint8_t update_data_array(uint16_t new_data, uint16_t data[], uint8_t data_length, uint8_t index)
{
  data[index] = new_data;
  index = (index + 1) % data_length;
  return index;
  /*
    //rotate the array to make room for new
    for (int i = 0; i < data_length - 1; i++)
    {
    data[i] = data[i + 1];
    }
    data[data_length - 1] = new_data;
  */
}

void display_heart_rate(int x_loc, int y_loc)
{
  load_bitmap(x_loc, y_loc, heart_bmp, heart_WIDTH, heart_HEIGHT);
  display.setTextSize(2);
  print_data(heart_WIDTH + x_loc + 2, y_loc, heart_rate);
}

void display_batt_status(int x_loc, int y_loc)
{
  uint16_t sensorValue = analogRead(ADC_BATTERY);//0 to 4095 is 0 to 4.3V
  if (batt_avg == 0)
  {
    batt_avg = sensorValue;
  }
  else
  {
    batt_avg = batt_avg * (1.00 - 0.01) + sensorValue * 0.01;
  }
  uint8_t batt_fill = (batt_avg - 3142) * 100 / (4090 - 3142); //3142 is about 3.3V, 2857 is about 3V (0%), 4000 is about 4.2V (100%)

  //8 pixels tall by 5 pixels wide battery indicator
  uint8_t pix_fill = batt_fill / 15;
  //generate battery symbol
  unsigned char batt_level[5] = {0x70, 0x00, 0x00, 0x00, 0x00};
  for (int i = 5; i < 40; i++)
  {
    int index = i / 8;
    char bitMask = 0x80 >> (i % 8);
    if ((i % 5 == 0) || (i % 5 == 4) || i > 35 || (pix_fill >= (40 - i) / 5))
    {
      batt_level[index] = batt_level[index] | bitMask;//turn on the bit
    }
  }
  load_bitmap(x_loc, y_loc, batt_level, 5, 8);
  display.setTextSize(1);
  print_data(5 + x_loc + 2, y_loc, batt_fill);
  if (batt_fill < 100)
  {
    display.print(F("%"));
  }
}

void print_data(int x_loc, int y_loc, uint8_t data)
{
  String chars[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  uint8_t divs[3] = {100, 10, 1};
  display.setCursor(x_loc, y_loc);
  display.setTextColor(WHITE);

  uint8_t modData = data;
  uint8_t printed_chars = 0;
  for (int i = 0; i < 3; i++)
  {
    uint8_t index = modData / divs[i];
    modData = modData - index * divs[i];
    if ((index == 0 && printed_chars != 0) || i == 2 || index > 0 )
    {
      display.print(chars[index]);
      printed_chars += 1;
    }
  }

}

void print_log_header()
{
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(F("Time[us],ECG[mV],Heart Rate[bpm],"));
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println(F("error opening datalog.txt"));
  }
}
