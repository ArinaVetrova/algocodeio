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
   - `remainChilds`: счётчик детей ожидающих шифрования (вместо `processedChilds` из v1)
   - `pool`: указатель на пул потоков для отправки задач шифрования
   - `mtx`: мьютекс для защиты `encryptedChilds` и `remainChilds` от гонок
   - наследует `std::enable_shared_from_this` — чтобы безопасно передавать `shared_ptr` на себя в задачи пула

2. **Добавлен метод `OnChildEncrypted`**
   Вызывается дочерним узлом когда он зашифрован. Под локом добавляет результат в `encryptedChilds` и уменьшает `remainChilds`. Если `remainChilds` стал 0 — все дети готовы, отправляет шифрование текущего узла в пул через `pool->Submit`.

3. **Вспомогательные функции**
   - `SubmitLeaf` — отправляет листовой узел в пул на немедленное шифрование
   - `EnqueueChildren` — создаёт `NodeEncryptionCollector` для каждого ребёнка и добавляет их в очередь с callback который вызывает `OnChildEncrypted` у родителя

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
Поток 1: encryptNode(subChild1) → OnChildEncrypted(child1) → remainChilds=0 → SubmitLeaf(child1)
Поток 2: encryptNode(subChild2) → OnChildEncrypted(child2) → remainChilds=0 → SubmitLeaf(child2)
Поток 1: encryptNode(child1) → OnChildEncrypted(root) → remainChilds=1
Поток 2: encryptNode(child2) → OnChildEncrypted(root) → remainChilds=0 → SubmitLeaf(root)
Поток 1: encryptNode(root) → callback1 → cv.notify_one()
Главный поток: просыпается → return res
```

6. **Тесты**: `testXmlEncryptorMt.cpp`

---

## Версия 3 – WorkStealingPool for xmlEncryptor (`xmlEncryptorWorkStealingPool.cpp`) - // TODO
