#include <pthread.h>
#include <queue>
#include <iostream>
#include <fstream>
#include <experimental/random>
#include <cstring>
#include <unistd.h>

// Глобальная переменная, для определения куда будут печататься логи:
// 1) Только в стандартный выходной поток, если путь пуст
// 2) В выходной поток и выходной файл, если путь до файла корректен
std::string path;

// Класс пациента, который хранит необходимую информацию о каждом пациенте больницы.
class Patient {
private:
    std::string name;
    std::string surname;
    // Тяжесть болезни пациента.
    // Значение может быть от 1 до 10, влияет на потраченное на пациента время.
    short illnessStage;
    // Тип жалобы пациента.
    // Значение может быть от 1 до 3,
    // где 1 - проблема с зубами
    // где 2 - проблема с органами
    // где 3 - проблема общего характера
    short illnessType;

    // Генератор для создания строки имени/фамилии.
    static std::string generator() {
        int n = std::experimental::randint(6, 12);
        std::string string;
        string += static_cast<char>('A' + std::experimental::randint(0, 25));
        for (int i = 0; i < n; ++i) {
            string += static_cast<char>('a' + std::experimental::randint(0, 25));
        }
        return string;
    }

    // Присваиваем имени и фамилии сгенерированные значения.
    void generateNames() {
        name = generator();
        surname = generator();
    }

public:

    // Getter к полю illnessStage, то есть тяжести болезни.
    short getIllnessStage() const {
        return illnessStage;
    }

    // Getter к полю illnessType, то есть типу болезни.
    short getIllnessType() const {
        return illnessType;
    }

    // Метод для отображения пациента в строковом эквиваленте.
    std::string toString() {
        return "Patient " + name + " " + surname +
               " Illness(type, stage) = (" + std::to_string(illnessType)
               + ", " + std::to_string(illnessStage) + ")";
    }

    // Конструктор от двух параметров.
    Patient(short type, short stage) {
        illnessType = type;
        illnessStage = stage;
        generateNames();
    }

    // Полный генератор пациента.
    // Генерирует значения для типа и тяжести болезни,
    // а далее вызывает конструктор от этих параметров.
    static Patient *generatePerson() {
        short type, stage;
        type = static_cast<short>(std::experimental::randint(1, 3));
        stage = static_cast<short>(std::experimental::randint(1, 10));
        auto *patient = new Patient(type, stage);
        return patient;
    }
};

// Класс врача и прототип для других классов врачей.
// Выступает готовым классом для дежурного врача.
class Doctor {
protected:
    // Предельное время регистрации клиента к другому врачу.
    double timeOfReceipt_ = 5;
public:
    // Представление врача в строковом эквиваленте.
    virtual std::string toString() {
        return "registration";
    }

    // Метод единый для всех типов врачей.
    // Возвращает округленное время в секундах, которое потратил сотрудник на пациента.
    // Значение времени определяется как:
    // time = максимальное время обслуживание * тяжесть болезни / 10.
    // То есть время может варьироваться от 10% до 100% от своего предельного значения.
    size_t getReceiptTime(Patient &patient) const {
        return static_cast<size_t>(timeOfReceipt_ * patient.getIllnessStage() / 10);
    }
};

// Класс стоматолога, наследующийся от врача.
// Для него переопределены предельное время и строковое представление.
class Dentist : public Doctor {
public:
    // Конструктор для переопределения предельного времени.
    Dentist() {
        timeOfReceipt_ = 20;
    }

    // Представление врача в строковом эквиваленте.
    std::string toString() override {
        return "dentist";
    }
};

// Класс хирурга, наследующийся от врача.
// Для него переопределены предельное время и строковое представление.
class Surgeon : public Doctor {
public:
    // Конструктор для переопределения предельного времени.
    Surgeon() {
        timeOfReceipt_ = 15;
    }

    // Представление врача в строковом эквиваленте.
    std::string toString() override {
        return "surgeon";
    }
};

// Класс терапевта, наследующийся от врача.
// Для него переопределены предельное время и строковое представление.
class Therapist : public Doctor {
public:
    // Конструктор для переопределения предельного времени.
    Therapist() {
        timeOfReceipt_ = 12;
    }

    // Представление врача в строковом эквиваленте.
    std::string toString() override {
        return "therapist";
    }
};

// Класс очереди в больницы.
// Моделирует работы очереди у регистрации и в кабинеты докторов.
class HospitalQueue {
protected:
    // Очередь в заданное место
    std::queue<Patient> queue{};
    // Тип врача, к которому стоит очередь
    Doctor *doctor = nullptr;
    // Мьютексы, для корректного работы потоков во время обращения к одной очереди.
    pthread_mutex_t addMutex{};
    pthread_mutex_t callMutex{};
    pthread_mutex_t checkMutex{};
    pthread_mutex_t printMutex{};
public:
    // Конструктор больничной очереди.
    // Инициализирует мьютексы и задает тип врача.
    explicit HospitalQueue(Doctor *doc) {
        doctor = doc;
        pthread_mutex_init(&addMutex, nullptr);
        pthread_mutex_init(&callMutex, nullptr);
        pthread_mutex_init(&checkMutex, nullptr);
        pthread_mutex_init(&printMutex, nullptr);
    }

    // Метод контролирующий доступ потоков при выводе в файл и консоль
    void printControl(const std::string &str) {
        if (path.empty()) {
            // Критическая секция с выводом в консоль
            pthread_mutex_lock(&printMutex);
            std::cout << str;
            pthread_mutex_unlock(&printMutex);
        } else {
            // Критическая секция с выводом в консоль и файл
            pthread_mutex_lock(&printMutex);
            std::cout << str;
            std::ofstream file(path, std::fstream::app);
            if (file.is_open()) {
                file << str;
                file.close();
            } else {
                std::cout << "Can't find file with name " << path << "\n";
                exit(0);
            }
            pthread_mutex_unlock(&printMutex);
        }
    }

    // Метод для возвращения размера очереди.
    size_t getSize() {
        return queue.size();
    }

    // Добавление пациента в очередь.
    void addPatient(Patient patient) {
        // Критическая секция с добавлением пациента в очередь и выводом лога в потоки.
        pthread_mutex_lock(&addMutex);
        queue.push(patient);
        std::string str = patient.toString() + " got in queue to " +
                          doctor->toString() + ".\n";
        printControl(str);
        pthread_mutex_unlock(&addMutex);
    }

    // Метод для допуска потока к действиям с очередью, если она не пуста.
    bool callPatientChecker() {
        // Критическая секция с проверкой пустоты очереди
        pthread_mutex_lock(&checkMutex);
        bool value = !queue.empty();
        pthread_mutex_unlock(&checkMutex);
        return value;
    }

    // Метод для вызова пациента в кабинет или к стойке регистрации.
    Patient callPatient() {
        // Критическая секция с удалением пациента из очереди и выводом лога в потоки.
        pthread_mutex_lock(&callMutex);
        Patient patient = queue.front();
        queue.pop();
        std::string str = doctor->toString() + " called out " + patient.toString() + ".\n";
        printControl(str);
        pthread_mutex_unlock(&callMutex);
        // Производим "работу" с пациентом без блокировки мьютекса,
        // чтобы другие потоки могли работать.
        // Поток спит время, высчитанное по формуле для текущего врача и пациента.
        sleep(doctor->getReceiptTime(patient));
        str = patient.toString() + " has left " + doctor->toString() + ".\n";
        printControl(str);
        return patient;
    }
};

// Класс больницы, собирающий в себе все типы очередей.
// Методы для работы потоков являются статическими, чтобы соответствовать сигнатуре.
class Hospital {
public:
    // Больничные очереди соответствующих типов.
    inline static HospitalQueue *registrationQueue = new HospitalQueue(new Doctor());
    inline static HospitalQueue *dentistQueue = new HospitalQueue(new Dentist());
    inline static HospitalQueue *surgeonQueue = new HospitalQueue(new Surgeon());
    inline static HospitalQueue *therapistQueue = new HospitalQueue(new Therapist());

    // Метод для добавления пациента в очередь регистрации.
    // Используется при вводе исходных данных.
    static void newPatient(Patient *patient) {
        registrationQueue->addPatient(*patient);
    }

    // Получаем количество пациентов, оставшихся в больнице.
    // Необходимо, чтобы закончить работу потоков, когда все будут обслужены.
    static size_t getAllPatients() {
        return registrationQueue->getSize() + dentistQueue->getSize() +
               surgeonQueue->getSize() + therapistQueue->getSize();
    }

    // Метод для распределения пациента к врачу в соответствии с типом его болезни.
    static void *distributePatient(void *) {
        // Проверка, что очередь не пуста.
        if (registrationQueue->callPatientChecker()) {
            Patient result = registrationQueue->callPatient();
            switch (result.getIllnessType()) {
                case 1:
                    dentistQueue->addPatient(result);
                    break;
                case 2:
                    surgeonQueue->addPatient(result);
                    break;
                case 3:
                    therapistQueue->addPatient(result);
                    break;
            }
        }
        return nullptr;
    }

    // Метод для вызова пациента к врачу.
    // Параметр определяется тип врача, который вызывает людей из очереди.
    static void *callPatient(void *option) {
        HospitalQueue *queue;
        short value = *((short *) option);
        switch (value) {
            case 1:
                queue = dentistQueue;
                break;
            case 2:
                queue = surgeonQueue;
                break;
            case 3:
                queue = therapistQueue;
                break;
            default:
                return nullptr;
        }
        if (queue->callPatientChecker()) {
            queue->callPatient();
        }
        return nullptr;
    }
};

// Метод для считывания количества пациентов из консоли.
void readData() {
    int n;
    std::cout << "Enter number of patients:";
    try {
        std::cin >> n;
        if (n < 1 || n > 100) {
            std::cout << "Incorrect value for patients number! Available values are [1, 100].\n";
            exit(0);
        }
        // Генерация данных для пациентов.
        for (int i = 0; i < n; ++i) {
            Hospital::newPatient(Patient::generatePerson());
        }
    } catch (std::exception &e) {
        std::cout << "Incorrect value for patients number! Available values are [1, 100].\n";
    }
}

// Метод для считывания количества пациентов из командной строки.
void readDataFromCommandLine(char **argv) {
    try {
        int n = std::stoi(argv[1]);
        if (n < 1 || n > 100) {
            std::cout << "Incorrect value for patients number! Available values are [1, 100].\n";
            exit(0);
        }
        // Генерация данных для пациентов.
        for (int i = 0; i < n; ++i) {
            Hospital::newPatient(Patient::generatePerson());
        }
    } catch (std::exception &e) {
        std::cout << "Incorrect value for patients number! Available values are [1, 100].\n";
    }
}

// Метод для генерации количества пациентов и их самих.
void generateData() {
    // Генерация количества пациентов.
    int n = std::experimental::randint(10, 100);
    // Генерация данных для пациентов.
    for (int i = 0; i < n; ++i) {
        Hospital::newPatient(Patient::generatePerson());
    }
}

// Метод для считывания количества пациентов из файла.
void readDataFromFile(char **argv) {
    std::ifstream file(argv[1]);
    if (file.is_open()) {
        int n;
        file >> n;
        if (n < 1 || n > 100) {
            std::cout << "Incorrect value for patients number! Available values are [1, 100].\n";
            exit(0);
        }
        file.close();
        // Генерация данных для пациентов.
        for (int i = 0; i < n; ++i) {
            Hospital::newPatient(Patient::generatePerson());
        }
    } else {
        std::cout << "Can't find file with name " << argv[1] << "\n";
        exit(0);
    }
}

// Метод для очистки файла записи перед записью новых логов.
void clearWriteFile() {
    std::ofstream file(path, std::ofstream::trunc);
    file.close();
}

int main(int argc, char **argv) {
    if (argc < 1 || argc > 3) {
        std::cout << "Incorrect number of parameters.\n";
        return 1;
    }
    // Выбор опции считывания количества пациентов
    // в зависимости от количества и типа заданных параметров.
    if (argc == 1) {
        readData();
    } else if (argc == 2) {
        readDataFromCommandLine(argv);
    } else if (strcmp(argv[1], "-g") == 0) {
        // Задаем путь для записи логов.
        path = argv[2];
        generateData();
        // Очищаем выходной файл, чтобы там были только новые логи.
        clearWriteFile();
    } else {
        // Задаем путь для записи логов.
        path = argv[2];
        readDataFromFile(argv);
        // Очищаем выходной файл, чтобы там были только новые логи.
        clearWriteFile();
    }
    // Массив потоков.
    pthread_t threads[5]{};
    // Потоки будут работать, пока больница не опустеет.
    while (Hospital::getAllPatients() != 0) {
        try {
            // Создание 2 потоков для дежурных врачей.
            for (int i = 0; i < 2; ++i) {
                pthread_create(&threads[i], nullptr, &Hospital::distributePatient, nullptr);
            }
            // Создание по потоку на каждого врача.
            for (int i = 2; i < 5; ++i) {
                // Передача типизирующего параметра врача.
                short *arg;
                arg = static_cast<short *>(malloc(sizeof(*arg)));
                *arg = static_cast<short>(i - 1);
                pthread_create(&threads[i], nullptr, &Hospital::callPatient, arg);
            }
            // Ожидание завершения работы потоков перед созданием новых.
            for (unsigned long long thread: threads) {
                pthread_join(thread, nullptr);
            }
        } catch (std::invalid_argument &e) {
            continue;
        }
    }
    std::cout << "Day finished!\n";
}
