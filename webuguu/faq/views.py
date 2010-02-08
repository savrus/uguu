# -*- coding: utf-8 -*-
# webuguu.vfs.views - vfs view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.shortcuts import render_to_response
import string
from webuguu.common import known_filetypes, known_protocols

def bold(x):
    return u"<b>%s</b>" % x

def strargs(args):
    return string.join([bold(x) for x in args], ", ")

sizedesc = u" Размером является десятичное число с последующим спецификатором. Спецификатором может быть одно из: tb, gb, mb, kb, byte."

ru_multiarg = u" Moжно задать несколько аргументов через запятую, но не разделяя пробелом."

ru_orders = {
    'avl':      u"доступность шары",
    'scan':     u"время последнего сканирования",
    'uptime':   u"время последнего изменения доступности",
    'proto':    u"протокол",
    'net':      u"сеть",
    'host':     u"имя хоста",
    'size':     u"размер файла",
    'name':     u"имя файла",
    'type':     u"тип файла",
    'sharesize':     u"размер шары целиком",
}

def ru_order_desc():
    return string.join([ u"%s (%s)" % (bold(x), ru_orders[x]) for x in ru_orders.keys()], ", ")

ru_qoptions = (
    {'option': "fullpath",
     'args': strargs(["yes", "no"]),
     'desc': u"Признак поиска в полном пути",
    },
    {'option': "max",
     'args': strargs(["100gb", "256.8mb"]),
     'desc': u"Максимальный размер файла." + sizedesc,
    },
    {'option': "min",
     'args': "<i>integer</i>",
     'desc': u"Минимальный размер файла." + sizedesc
    },
    {'option': "type",
     'args': strargs(tuple(known_filetypes) + ("dir",)),
     'desc': u"Искать только среди файлов данного типа." + ru_multiarg
    },
    {'option': "proto",
     'args': strargs(known_protocols),
     'desc': u"Искать только в шарах с заданым протоколом." + ru_multiarg,
    },
    {'option': "host",
     'args': "<i>hostname</i>",
     'desc': u"Искать только в шарах с заданым именем хоста." + ru_multiarg,
    },
    {'option': "port",
     'args': "<i>integer</i>",
     'desc': u"Искать только в шарах с заданым портом. Для шар на дефолтном порте нужно указывать значение 0." + ru_multiarg,
    },
    {'option': "network",
     'args': "<i>network</i>",
     'desc': u"Искать только в заданной сети." + ru_multiarg,
    },
    {'option': "avl",
     'args': strargs(["online", "offline"]),
     'desc': u"Искать только в доступных/недоступных шарах",
    },
    {'option': "order",
     'args': strargs(ru_orders.keys()),
     'desc': u"Сортировать результаты в заданном порядке. Каждый аргумент можно использовать с суффиксом <b>.d</b>, это будет означать сортировку в обратном порядке." + ru_multiarg + u"Значение у аргументов следующее: " + ru_order_desc(),
    },
)


def ru(request):
    return render_to_response("faq/ru.html",
        {'options': ru_qoptions,
         'gpage': "http://code.google.com/p/uguu",
         'ggroup': "http://groups.google.com/group/uguu",
        })

