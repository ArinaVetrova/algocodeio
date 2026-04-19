## Version 1 – исправленный DicsreteSampler (`discreteSampler.cpp`)

1. **Класс сделан шаблонным**
   Класс теперь является шаблоном и принимает тип `T` для значений objects_. Для весов значение остается float.

2. **Добавлена функция Add()**
   Для ее корреткной работы std::vector<std::pair<int, float>> objectsWithWeights заменен на 2 вектора:
    ```
    std::vector<T> objects_;
    std::vector<float> cumulativeWeights_;
    ```

    при добавлении объекта его значение и вес добавляется в соотв. вектор, totalWeight обновляется, добавляя новое значение Weight

    Исправлен метод получения Sample (выполняется домножение на totalWeight)

3. **Конструктор принимает функтор randomGenerator**
    Это позволяет сделать результат тестов предсказуемым
