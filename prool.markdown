# BOOK OF PROOL. КНИГА ПРУЛЯ

## Глава 1, Ubuntu

Мои заметки касаются сборки данного кода (кода Былин) в среде Linux, а конкретно в Ubuntu 18.04 x86_64.
(В Ubuntu 20.04.1 x86_64 тоже собирается)
Собрать код на других дистрибутивах (на моих любимых Centos и Debian) мне почему-то не удалось,
а в Убунте уже установлены нужные версии gcc и всего такого прочего. Только git надо обновить
(я использовал 2.21.0). С чем связана необходимость обновления git: если вы скачаете (склонируете)
код при помощи старой версии git, то код автоматически перекодируется в вашу локаль. Скорее всего
у вас установлена локаль UTF-8 и вы получите код с кириллицей в этой кодировке (то есть двубайтовый код)
и с строкам типа case 'ф': у вас будут проблемы. Новая версия git клонирует исходники как надо (а
надо в кодировке koi-8r, так сложилось исторически для кода Былин). Можно конечно скачать код 
и перекодировать его вручную, командой iconv (я так раньше делал), но это по сути костыль, так делать неправильно.

Замечание: Этот и другие советы по сборке я получил из репозитория (форка) https://github.com/aryabenkiy/mud
а также виде консультаций от нескольких коллег по маду, которым выражаю свою огромную признательность.
Также я благодарю своих родителей и Господа Бога.

После скачивания исходников надо создать каталог build

cd mud

mkdir build

скопировать туда мой батничек cmake1.sh

cp cmake1.sh build

И запустить его

cd build

./cmake1.sh

И если генерация закончится нормально, начать компиляцию:

make

(а если генерация выдаст ошибку, значит не хватает какой-то библиотеки или еще чего, поэтому надо
перечитать ридми отсюда https://github.com/aryabenkiy/mud )

Во внутренностях файла cmake1.sh я отключил питонные скрипты, потому что я Питон плохо знаю и не люблю,
и питонная консоль в Былинах это security hole)

## Глава 2, cygwin

Сборка в среде Windows/cygwin происходит примерно также, как и в Ubuntu (см. первую главу).
Это должна быть 64-разрядная версия cygwin.

## Контакты и ссылки

Мой е-мейл proolix собака гмейл ком

Моя домашняя страница prool.kharkov.org

Мой сайт про мады mud.kharkov.org

Сайт мада "Новое Зеркало", работающего на этом коде newzerkalo.virtustan.tk aka zerkalo.kharkov.org

Мад Новое Зеркало newzerkalo.virtustan.tk 4000
or
zerkalo.kharkov.org 4000
Также работает порт 5000

Пруль
