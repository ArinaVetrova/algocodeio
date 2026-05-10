## Version 1 – исправленный DicsreteSampler (`discreteSampler.cpp`)

1. **Класс сделан шаблонным**
   Класс теперь является шаблоном и принимает тип `T` для значений objects. Для весов значение остается float.

2. **Добавлена функция Add()**
   Для ее корреткной работы std::vector<std::pair<int, float>> objectsWithWeights заменен на 2 вектора:
    ```
    std::vector<T> objects;
    std::vector<float> cumulativeWeights;
    ```

    при добавлении объекта его значение и вес добавляется в соотв. вектор, totalWeight обновляется, добавляя новое значение Weight

    Исправлен метод получения Sample (выполняется домножение на totalWeight)

3. **Конструктор принимает функтор randomGenerator**
    Это позволяет сделать результат тестов предсказуемым

4. **Файл тестов** - `testDiscreteSampler.cpp`

## Version 2 – многопоточная версия (`discreteSamplerMt.h`)

### Изменения по сравнению с Version 1

1. **Добавлен `shared_mutex`**
   
   `Sample()` использует `shared_lock`, `Add()` использует `unique_lock` - много потоков могут читать одновременно, но запись монопольная.

2. **Потокобезопасный генератор**
   
   В продакшен-режиме (без `randomGenerator`) используется `thread_local std::mt19937` — у каждого потока свой экземпляр, конкуренции нет.

3. **Тесты** — `testDiscreteSamplerMt.cpp`
