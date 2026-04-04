## Version 1 – однопоточный xmlEncryptor (`xmlEncryptor.h`)

1. **Добавлена вспомогательная структура NodeEncryptionCollector**
   Хранит данные для шифрования XML-узла.
   - `node`: указатель на узел XML, содержащий тег, содержимое узла и вектор его прямых потомков
   - `path`: путь до узла в виде вектора строковых представлений (теги от родительского к текущему)
   - `processedChilds`: счётчик уже отправленных в стек детей
   - `encryptedChilds`: вектор пар (имя дочернего узла и его зашифрованный текст)
   - `onDone`: callback который вызывается когда узел зашифрован, кладёт результат в `encryptedChilds` родителя

2. **Реализация функции `encryptNode` в целях тестирования**
   По условиям задания функция реализована снаружи и её реализовывать не надо. Реализована как построение строки в порядке шифрования для упрощения тестирования.

3. **Реализация функции `encryptXmlTree`**
   Алгоритм построен на итеративном обходе дерева XML с помощью `std::stack`.
   Для каждого посещаемого узла в stack помещается объект `NodeEncryptionCollector`, в котором сохраняется путь, ссылка на узел, счётчик уже обработанных детей, контейнер для зашифрованных детей и callback `onDone`.

   Пока `processedChilds` меньше количества детей текущего узла, берём следующий дочерний узел, формируем для него новый путь и помещаем новый `NodeEncryptionCollector` в stack с callback который кладёт результат в `encryptedChilds` родителя.

   Когда у узла нет необработанных детей — вызываем `encryptNode` и передаём результат в `onDone`. Обработанный узел удаляется из stack. Когда stack пуст — root зашифрован и `res` возвращается вызывающему коду.

4. **Тесты**: `testXmlEncryptor.cpp`

---

## Версия 2 – Thread-safe xmlEncryptor (`xmlEncryptorMt.cpp`)

1. **Расширена структура NodeEncryptionCollector**
   По сравнению с однопоточной версией добавлены поля для многопоточной работы:
   - `remainChilds`: атомарный счётчик детей ожидающих шифрования (вместо `processedChilds` из v1)
   - `pool`: указатель на пул потоков для отправки задач шифрования
   - наследует `std::enable_shared_from_this` — чтобы безопасно передавать `shared_ptr` на себя в задачи пула
   - `encryptedChilds` инициализируется через `resize` вместо `reserve` — чтобы каждый ребёнок писал результат строго по своему индексу и порядок детей был гарантирован независимо от порядка завершения потоков

2. **Добавлен метод `OnChildEncrypted`**
   Вызывается дочерним узлом когда он зашифрован. Пишет результат в `encryptedChilds[childIdx]` по индексу без мьютекса — каждый поток пишет в свой индекс. Уменьшает `remainChilds` через `fetch_sub(1, memory_order_acq_rel)` — минимальный memory order который гарантирует что запись в `encryptedChilds` у всех потоков видна последнему потоку перед запуском `encryptNode`. Если `fetch_sub` вернул 1 — все дети готовы, отправляет шифрование текущего узла в пул через `pool->Submit`.

3. **Вспомогательные функции**
   - `SubmitLeaf` — отправляет листовой узел в пул на немедленное шифрование
   - `EnqueueChildren` — создаёт `NodeEncryptionCollector` для каждого ребёнка с его индексом и добавляет в очередь с callback который вызывает `OnChildEncrypted(childIdx, ...)` у родителя

4. **Реализация функции `encryptXmlTree`**
   Обход дерева и шифрование разделены в отличие от однопоточной версии.

   Главный поток обходит дерево через `std::queue` — лист попадает в `SubmitLeaf`, нелист в `EnqueueChildren`. После обхода главный поток блокируется на `condition_variable`.

   Листья шифруются параллельно в пуле. Каждый готовый узел вызывает `OnChildEncrypted` у родителя. Последний ребёнок запускает шифрование родителя через `pool->Submit`. Цепочка поднимается до root.

   Callback root'а записывает результат в `res` и делает `cv.notify_one` — главный поток просыпается и возвращает результат.

5. **Пример для дерева:**
```
root → child1 → subChild1
     → child2 → subChild2
```
```
Главный поток: BFS обход → SubmitLeaf(subChild1), SubmitLeaf(subChild2) → cv.wait()
Поток 1: encryptNode(subChild1) → OnChildEncrypted(0, child1) → remainChilds=0 → Submit(child1)
Поток 2: encryptNode(subChild2) → OnChildEncrypted(1, child2) → remainChilds=0 → Submit(child2)
Поток 1: encryptNode(child1) → OnChildEncrypted(0, root) → remainChilds=1
Поток 2: encryptNode(child2) → OnChildEncrypted(1, root) → remainChilds=0 → Submit(root)
Поток 1: encryptNode(root) → callback1 → cv.notify_one()
Главный поток: просыпается → return res
```

6. **Тесты**: `testXmlEncryptorMt.cpp`

---

## Версия 3 – WorkStealingPool for xmlEncryptor (`workStealingPool.cpp`)

1. Принцип работы

**WorkStealingPool** — пул потоков, где каждый поток имеет свою локальную очередь задач. Если у потока нет задач — он "ворует" задачу из очереди другого потока.

2. Структура

- **WorkerQueue**: очередь задач (`std::deque`), мьютекс, условная переменная, поток (`std::jthread`)
- **workers**: `vector<unique_ptr<WorkerQueue>>` - uniqur_ptr выбран, т.к. содержимое WorkerQueue может быть только перемещено из-за мьютека и cv
- **activeTasks**: атомарный счётчик активных задач для ожидания завершения всех задач
- **submitIdx**: атомарный индекс в `vector<unique_ptr<WorkerQueue>>` для round-robin распределения

3. Алгоритм работы потока

```cpp
while (!stop_requested) {
    // 1. Берём задачу из своей очереди (с конца)
    task = tryPopOwn();
    
    // 2. Если нет - воруем у других (с начала)
    if (!task) task = trySteal();
    
    // 3. Выполняем или засыпаем
    if (task) task();
    else cv.wait();
}
```
4. Тесты -  `testWorlStealingPool.cpp`
