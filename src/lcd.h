#ifndef LCD_H
#define LCD_H

#include <LiquidCrystal_I2C.h>
#include <vector>

void displayLCD(LiquidCrystal_I2C& lcd, const std::vector<String>& dataList, int duration = 2000) {
    lcd.clear();
    for (size_t i = 0; i < dataList.size() && i < 2; i++) {
        lcd.setCursor(0, i);
        lcd.print(dataList[i]);
    }
    delay(duration);
    lcd.clear();
}
#endif
