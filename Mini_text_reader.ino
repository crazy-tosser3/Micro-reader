#include <GyverOLED.h>
#include <GyverButton.h>
#include <SPI.h>
#include <SD.h>

GButton up(3, HIGH_PULL);
GButton down(5, HIGH_PULL);
GButton ok(9, HIGH_PULL);

#define MAX_FILES 7          // Максимальное количество файлов для отображения в меню
#define FILENAME_LEN 13      // Ограничение на длину имени файла (8.3 формат)
int contr = 100;
int font_s = 1;

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
File myFile;
File root;
char fileList[MAX_FILES][FILENAME_LEN];  // Массив для хранения имен файлов
int fileCount = 0;  // Количество файлов, найденных на SD
bool isReadingFile = false;  // Флаг для режима чтения файла
uint8_t pointer = 0;  // Указатель для меню

void setup() {
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  delay(50);

  oled.init();
  oled.setContrast(contr);
  oled.clear();

  SDcardInit();
}

void loop() {
  up.tick();
  down.tick();
  ok.tick();

  static uint8_t lastPointer = 100;  // Переменная для хранения предыдущей позиции указателя

  if (isReadingFile) {
    // Ждем нажатия кнопки "ОК" для выхода из режима чтения файла
    if (ok.isClick()) {
      isReadingFile = false;  // Выход из режима чтения файла
      drawMenu(pointer);  // Возвращаемся в меню
    }
    return;  // Прекращаем выполнение цикла, пока читаем файл
  }

  // Обработка нажатия кнопок "вверх" и "вниз"
  if (up.isClick()) {
    pointer = constrain(pointer - 1, 0, fileCount - 1);  // Ограничиваем указатель количеством файлов
  }

  if (down.isClick()) {
    pointer = constrain(pointer + 1, 0, fileCount - 1);  // Ограничиваем указатель количеством файлов
  }

  // Обработка нажатия кнопки "ОК"
  if (ok.isClick()) {
    isReadingFile = true;
    openFile(fileList[pointer]);  // Открываем файл
  }

  // Перерисовываем экран только если указатель изменился
  if (pointer != lastPointer) {
    drawMenu(pointer);
    lastPointer = pointer;
  }
}

// Функция отрисовки меню
void drawMenu(uint8_t pointer) {
  oled.clear();
  oled.home();
  oled.setScale(font_s);

  // Выводим список файлов на экран
  for (int i = 0; i < fileCount; i++) {
    oled.setCursor(8, i);
    oled.print(fileList[i]);
  }

  oled.home();
  printPointer(pointer);
  oled.update();
}

void printPointer(uint8_t ptr) {
  oled.setCursor(-1, ptr);
  oled.print(">");
  oled.setCursor(20 * 5.0, ptr);
  oled.print("<");
}

// Функция для чтения и отображения содержимого выбранного файла с прокруткой
void openFile(const char* filename) {
  oled.clear();
  oled.home();

  // Открываем файл для чтения
  myFile = SD.open(filename);
  if (myFile) {
    char buffer[21];  // Буфер для строки (по ширине экрана OLED)
    int lineCount = 0;  // Переменная для подсчета строк на экране
    int currentLine = 0;  // Переменная для отслеживания текущей строки

    // Читаем файл построчно
    while (myFile.available()) {
      lineCount = 0;  // Сбрасываем счетчик строк
      oled.clear();
      oled.home();

      // Печатаем 7 строк, начиная с текущей
      while (lineCount < 7 && myFile.available()) {
        int len = myFile.readBytesUntil('\n', buffer, sizeof(buffer) - 1);  // Читаем строку
        buffer[len] = '\0';  // Завершаем строку нулевым символом
        
        oled.setCursor(0, lineCount);  // Устанавливаем курсор на новую строку
        oled.print(buffer);  // Выводим строку на экран
        lineCount++;
      }
      oled.update();

      // Ждем нажатия кнопки для прокрутки
      while (true) {
        up.tick();
        down.tick();
        ok.tick();
        
        if (up.isClick() && currentLine > 0) {
          currentLine--;  // Прокрутка вверх
          break;  // Выходим из внутреннего цикла, чтобы перерисовать экран
        }
        
        if (down.isClick() && myFile.available()) {
          currentLine++;  // Прокрутка вниз
          break;  // Выходим из внутреннего цикла, чтобы перерисовать экран
        }
        
        if (ok.isClick()) {
          myFile.close();  // Закрываем файл после выхода
          return;  // Выходим из функции
        }
      }

      // Перемещаем указатель на нужную строку в файле
      myFile.seek(currentLine * (sizeof(buffer) + 1)); // Устанавливаем указатель на нужную строку
    }

    myFile.close();  // Закрываем файл после завершения чтения
  } else {
    oled.print("File error!");
    oled.update();
    delay(2000);
  }

  isReadingFile = false;  // Возвращаемся в основное меню после выхода из файла
}

// Инициализация SD-карты и сбор списка файлов
void SDcardInit() {
  oled.setScale(font_s);
  oled.home();
  oled.clear();

  oled.print("Инициализация mSD");
  if (!SD.begin(4)) {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Ошибка инициализации!");
    while (1);
  }
  oled.clear();
  oled.setCursor(0, 0);
  oled.print("Успех!");
  delay(2000);
  oled.clear();

  scanSDcard();  // Сканируем файлы на SD-карте
}

// Функция для сканирования файлов на SD-карте
void scanSDcard() {
  root = SD.open("/");
  fileCount = 0;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      // Сохраняем имя файла в список, если это файл (и не папка)
      strncpy(fileList[fileCount], entry.name(), FILENAME_LEN - 1);
      fileList[fileCount][FILENAME_LEN - 1] = '\0';  // Добавляем нулевой символ в конец строки

      fileCount++;  // Увеличиваем счетчик файлов
      if (fileCount >= MAX_FILES) break;  // Если файлов больше, чем может уместиться на экране, выходим
    }
    entry.close();
  }
}
