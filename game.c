#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define COLS_COUNT 80  // Ранее STL
#define ROWS_COUNT 25  // Ранее STR

// Прототипы
int choose_fill_option(int field[][COLS_COUNT]);
int fill_field(int field[][COLS_COUNT], int option);
int check_empty_field(int field[][COLS_COUNT]);
int adjust_speed(char control, int *stop, int current_delay);
void compute_next_generation(int current[][COLS_COUNT], int next[][COLS_COUNT]);
int count_neighbors(int field[ROWS_COUNT][COLS_COUNT], int r, int c);
int next_cell_state(int neighbors, int is_alive);
int compare_fields(int field1[ROWS_COUNT][COLS_COUNT], int field2[ROWS_COUNT][COLS_COUNT]);
void copy_field(int source[][COLS_COUNT], int destination[][COLS_COUNT]);
void print_field(int field[][COLS_COUNT]);
void convert_to_graphics(int numeric_field[][COLS_COUNT], int graphic_field[][COLS_COUNT]);
int read_from_file(int field[][COLS_COUNT]);
void print_error(int error_code);

int main(void) {
    srand((unsigned)time(NULL));  // Инициализируем ГПСЧ

    int field[ROWS_COUNT][COLS_COUNT];
    int error_code = choose_fill_option(field);
    if (error_code != 0) {
        print_error(error_code);
        return 1;
    }

    // Если дошли сюда, поле заполнено корректно
    int next[ROWS_COUNT][COLS_COUNT];
    int graphic[ROWS_COUNT][COLS_COUNT];

    int stop = 0;         // Флаг остановки
    int no_changes = 0;   // Флаг, что поколение не изменяется
    int delay_us = 400000; // Задержка в микросекундах

    // Инициализация ncurses
    initscr();
    noecho();
    nodelay(stdscr, TRUE);

    while (!stop) {
        char user_input = getch();  // Считываем символ, если есть

        // Проверка, не опустело ли поле
        if (check_empty_field(field) == 0) {
            stop = 1;
        }

        // Корректируем скорость/завершение
        delay_us = adjust_speed(user_input, &stop, delay_us);

        // Считаем новое поколение
        compute_next_generation(field, next);

        // Проверяем, отличается ли новое поколение от старого
        if (compare_fields(field, next) == 0) {
            no_changes = 1;
        } else {
            no_changes = 0;
        }

        // Перекладываем новое поколение в текущее
        copy_field(next, field);

        // Готовим «графическую» матрицу (пробел/символ)
        convert_to_graphics(field, graphic);

        // Выводим результат
        print_field(graphic);
        printw("\nControls:\n");
        printw("  1 -> Speed++\n");
        printw("  2 -> Speed--\n");
        printw("  3 -> Default speed\n");
        printw("  4 -> Exit\n");
        printw("Current speed (microseconds): %d\n", delay_us);

        if (no_changes) {
            printw("\nNO CHANGES\n");
        }

        refresh();
        usleep(delay_us);
        clear();
    }

    endwin();  // Возвращаемся из режима ncurses
    return 0;
}

// Функция выбора варианта заполнения
int choose_fill_option(int field[][COLS_COUNT]) {
    printf("Выберите, как заполнить поле:\n");
    printf("  1. Случайно\n");
    printf("  2. Ввод с stdin\n");
    printf("  3. Готовый вариант (из файла)\n");
    int choice;
    if (scanf("%d", &choice) != 1) {
        return 1; // Некорректный ввод
    }

    int error = 0;
    if (choice == 1 || choice == 2) {
        error = fill_field(field, choice);
    } else if (choice == 3) {
        error = read_from_file(field);
    } else {
        error = 1; // Неверный вариант
    }
    return error;
}

// Заполнение матрицы
int fill_field(int field[][COLS_COUNT], int option) {
    if (option == 1) {
        // Случайное заполнение
        for (int r = 0; r < ROWS_COUNT; r++) {
            for (int c = 0; c < COLS_COUNT; c++) {
                field[r][c] = rand() % 2;
            }
        }
        return 0;
    } else {
        // Ручной ввод
        printf("Введите элементы матрицы (25 строк по 80 столбцов):\n");
        for (int r = 0; r < ROWS_COUNT; r++) {
            for (int c = 0; c < COLS_COUNT; c++) {
                if (scanf("%d", &field[r][c]) != 1) {
                    return 1; // Ошибка ввода
                }
            }
        }
        return 0;
    }
}

// Проверка, есть ли живые клетки на поле
int check_empty_field(int field[][COLS_COUNT]) {
    int sum = 0;
    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            sum += field[r][c];
        }
    }
    return sum; // Если 0 - поле пустое
}

// Корректировка скорости
int adjust_speed(char control, int *stop, int current_delay) {
    switch (control) {
        case '1':
            current_delay += 35000; // увеличить задержку -> медленнее
            break;
        case '2':
            current_delay -= 35000; // уменьшить задержку -> быстрее
            if (current_delay < 15000) {
                current_delay = 15000;
            }
            break;
        case '3':
            current_delay = 400000; // дефолт
            break;
        case '4':
            *stop = 1;
            break;
        default:
            // если ничего не нажали или нажат другой символ, не меняем скорость
            break;
    }
    return current_delay;
}

// Вычисление нового поколения
void compute_next_generation(int current[][COLS_COUNT], int next[][COLS_COUNT]) {
    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            int nb = count_neighbors(current, r, c);
            next[r][c] = next_cell_state(nb, current[r][c]);
        }
    }
}

// Подсчёт соседей с учётом тороидальности
int count_neighbors(int field[ROWS_COUNT][COLS_COUNT], int r, int c) {
    int sum = 0;

    // Индексы соседей
    int r_up    = (r == 0) ? (ROWS_COUNT - 1) : (r - 1);
    int r_down  = (r == ROWS_COUNT - 1) ? 0 : (r + 1);
    int c_left  = (c == 0) ? (COLS_COUNT - 1) : (c - 1);
    int c_right = (c == COLS_COUNT - 1) ? 0 : (c + 1);

    sum += field[r_up][c_left];
    sum += field[r_up][c];
    sum += field[r_up][c_right];
    sum += field[r][c_right];
    sum += field[r_down][c_right];
    sum += field[r_down][c];
    sum += field[r_down][c_left];
    sum += field[r][c_left];

    return sum;
}

// Правила игры "Жизнь"
int next_cell_state(int neighbors, int is_alive) {
    // is_alive = 1 -> клетка жива
    // neighbors = число соседей
    if (is_alive) {
        if (neighbors == 2 || neighbors == 3) {
            return 1; // остаётся жить
        } else {
            return 0; // умирает
        }
    } else {
        if (neighbors == 3) {
            return 1; // оживает
        } else {
            return 0; 
        }
    }
}

// Сравнение двух поколений
int compare_fields(int field1[ROWS_COUNT][COLS_COUNT], int field2[ROWS_COUNT][COLS_COUNT]) {
    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            if (field1[r][c] != field2[r][c]) {
                return 1; // Есть хотя бы одно отличие
            }
        }
    }
    return 0; // Нет отличий
}

// Копирование матриц
void copy_field(int source[][COLS_COUNT], int destination[][COLS_COUNT]) {
    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            destination[r][c] = source[r][c];
        }
    }
}

// Печать на экран через ncurses
void print_field(int field[][COLS_COUNT]) {
    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            printw("%c", (char)field[r][c]);
        }
        printw("\n");
    }
}

// Преобразование 0/1 в символы
void convert_to_graphics(int numeric_field[][COLS_COUNT], int graphic_field[][COLS_COUNT]) {
    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            // 0 -> пробел, 1 -> 'O'
            graphic_field[r][c] = (numeric_field[r][c] == 1) ? 'O' : ' ';
        }
    }
}

// Чтение из файла
int read_from_file(int field[][COLS_COUNT]) {
    printf("Введите номер варианта от 1 до 5:\n");
    int key;
    if (scanf("%d", &key) != 1) {
        return 1; // Некорректный ввод
    }
    if (key < 1 || key > 5) {
        return 1; // Некорректный номер варианта
    }

    // Можно было бы использовать массив строк с именами файлов
    const char* filenames[5] = {"1.txt", "2.txt", "3.txt", "4.txt", "5.txt"};
    FILE* f = fopen(filenames[key - 1], "r");
    if (!f) {
        return 2; // Не удалось открыть файл
    }

    for (int r = 0; r < ROWS_COUNT; r++) {
        for (int c = 0; c < COLS_COUNT; c++) {
            if (fscanf(f, "%d", &field[r][c]) != 1) {
                fclose(f);
                return 1; // Ошибка чтения
            }
        }
    }
    fclose(f);
    return 0;
}

// Вывод ошибок
void print_error(int error_code) {
    switch (error_code) {
        case 1:
            printf("Некорректный ввод\n");
            break;
        case 2:
            printf("Ошибка чтения файла\n");
            break;
        default:
            printf("Неизвестная ошибка\n");
            break;
    }
}
