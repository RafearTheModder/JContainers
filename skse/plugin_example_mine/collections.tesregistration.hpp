#pragma once

#include "skse/PapyrusVM.h"
#include "skse/PapyrusNativeFunctions.h"
#include "collections.h"
#include "autorelease_queue.h"
#include <boost/algorithm/string.hpp>

#define MESSAGE(...) // _DMESSAGE(__VA_ARGS__);

namespace collections {

    namespace tes_object {
        static const char * TesName() { return "JValue";}

        static HandleT retain(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            auto obj = collection_registry::getObject(handle);
            if (obj) {
                obj->retain();
                return handle;
            }

            return HandleNull;
        }

        static HandleT autorelease(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            autorelease_queue::instance().push(handle);
            return handle;
        }

        template<class T>
        static HandleT create(StaticFunctionTag *) {
            MESSAGE(__FUNCTION__);
            return T::create()->id;
        }

        template<class T>
        static HandleT object(StaticFunctionTag *tt) {
            MESSAGE(__FUNCTION__);
            return T::object()->id;
        }

        static void release(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            collection_base *obj = collection_registry::getObject(handle);
            if (obj) {
                obj->release();
            }
        }

        static bool isArray(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            collection_base *obj = collection_registry::getObject(handle);
            return obj && obj->_type == CollectionTypeArray;
        }

        static bool isMap(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            collection_base *obj = collection_registry::getObject(handle);
            return obj && obj->_type == CollectionTypeMap;
        }

        static HandleT readFromFile(StaticFunctionTag*, BSFixedString path) {
            if (path.data == nullptr)
                return 0;

            auto obj = json_parsing::readJSONFile(path.data);
            return  obj ? obj->id : 0;
        }

        static void writeToFile(StaticFunctionTag*, HandleT handle, BSFixedString path) {
            if (path.data == nullptr)  return;

            auto obj = collection_registry::getObject(handle);
            if (!obj)  return;
           
            char * data = json_parsing::createJSONData(*obj);
            if (!data) return;

            FILE *file = fopen(path.data, "w");
            if (!file) {
                free(data);
                return;
            }
            fwrite(data, 1, strlen(data), file);
            fclose(file);
            free(data);
        }

        template<class T>
        static typename Tes2Value<T>::tes_type resolveT(StaticFunctionTag*, HandleT handle, BSFixedString path) {
            auto obj = collection_registry::getObject(handle);
            if (!obj) return 0;

            Item itm = json_parsing::resolvePath(obj, path.data);
            return T(itm.readAs<T>());
        }

        void printMethod(const char *cname, const char *cargs) {
            //string args(cargs);

            const char * type2tes[][2] = {
                "HandleT", "int",
                "Index", "int",
                "BSFixedString", "string",
                "Float32", "float",
                "SInt32", "int",
            };
            std::map<string,string> type2tesMap ;
            for (int i = 0; i < sizeof(type2tes) / (2 * sizeof(char*));++i) {
                type2tesMap[type2tes[i][0]] = type2tes[i][1];
            }


            string name;

            vector<string> strings; // #2: Search for tokens
            boost::split( strings, string(cargs), boost::is_any_of(", ") );

            if (strings[0] != "void") {
                name += type2tesMap[strings[0]] + ' ';
            }

            name += "Function ";
            name += cname;
            name += "(";

            int argNum = 0;

            for (int i = 1; i < strings.size(); ++i, ++argNum) {
                if (strings[i].empty()) {
                    continue;
                }


                name += (type2tesMap.find(strings[i]) != type2tesMap.end() ? type2tesMap[strings[i]] : strings[i]);
                name += " arg";
                name += (char)((argNum - 1) + '0');
                if ((i+1) < strings.size())
                    name += ", ";
            }

            name += ") global native\n";

            printf(name.c_str());
        }

#define ARGS(...) __VA_ARGS__

        static bool registerFuncs(VMClassRegistry* registry) {

            #define REGISTER2(name, func, argCount,  ... /*types*/ ) \
                if (registry) { \
                registry->RegisterFunction( \
                new NativeFunction ## argCount <StaticFunctionTag, __VA_ARGS__ >(name, TesName(), func, registry)); \
                registry->SetFunctionFlags(TesName(), name, VMClassRegistry::kFunctionFlag_NoWait); \
                }\
                printMethod(name, #__VA_ARGS__);

            #define REGISTER(func, argCount,  ... /*types*/ ) REGISTER2(#func, func, argCount, __VA_ARGS__)

            REGISTER(release, 1, void, HandleT);
            REGISTER(retain, 1, HandleT, HandleT);
            REGISTER(autorelease, 1, HandleT, HandleT);

            REGISTER(readFromFile, 1, HandleT, BSFixedString);
            REGISTER(writeToFile, 2, void, HandleT, BSFixedString);

            REGISTER(isArray, 1, bool, HandleT);
            REGISTER(isMap, 1, bool, HandleT);

            REGISTER2("resolveVal", resolveT<Handle>, 2, HandleT, HandleT, BSFixedString);
            REGISTER2("resolveFlt", resolveT<Float32>, 2, Float32, HandleT, BSFixedString);
            REGISTER2("resolveStr", resolveT<BSFixedString>, 2, BSFixedString, HandleT, BSFixedString);
            REGISTER2("resolveInt", resolveT<SInt32>, 2, SInt32, HandleT, BSFixedString);

            return true;
        }
    }

    namespace tes_array {
        using namespace tes_object;

        static const char * TesName() { return "JArray";}

        typedef array::Index Index;

        static array* find(HandleT handle) {
            return collection_registry::getObjectOfType<array>(handle);
        }

        template<class T>
        static typename Tes2Value<T>::tes_type itemAtIndex(StaticFunctionTag*, HandleT handle, Index index) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            return index < obj->_array.size() ? obj->_array[index].readAs<T>() : 0;
        }

        template<class T>
        static void replaceItemAtIndex(StaticFunctionTag*, HandleT handle, Index index, typename Tes2Value<T>::tes_type item) {
            auto obj = find(handle);
            if (!obj) {
                return;
            }

            mutex_lock g(obj->_mutex);
            if (index < obj->_array.size()) {
                obj->_array[index] = Item((T)item);
            }
        }

        static void removeItemAtIndex(StaticFunctionTag*, Handle handle, Index index) {
            auto obj = find(handle);
            if (!obj) {
                return;
            }

            mutex_lock g(obj->_mutex);
            if (index < obj->_array.size()) {
                obj->_array.erase(obj->_array.begin() + index);
            }
        }

        template<class T>
        static void add(StaticFunctionTag*, HandleT handle, typename Tes2Value<T>::tes_type item) {
            MESSAGE(__FUNCTION__);
            print(item);
            auto obj = find(handle);
            if (obj) {
                mutex_lock g(obj->_mutex);
                obj->_array.push_back(Item((T)item));
            }
        }

        static Index count(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (obj) {
                mutex_lock g(obj->_mutex);
                return  obj->_array.size();
            }
            return 0;
        }

        static void clear(StaticFunctionTag*, HandleT handle) {
            auto obj = find(handle);
            if (obj) {
                mutex_lock g(obj->_mutex);
                obj->_array.clear();
            }
        }

        static bool registerFuncs(VMClassRegistry* registry) {

            MESSAGE("register array funcs");

            REGISTER2("create", create<array>, 0, HandleT);
            REGISTER2("object", object<array>, 0, HandleT);

            REGISTER(count, 1, Index, HandleT);

            REGISTER2("addFlt", add<Float32>, 2, void, HandleT, Float32);
            REGISTER2("addInt", add<SInt32>, 2, void, HandleT, SInt32);
            REGISTER2("addStr", add<BSFixedString>, 2, void, HandleT, BSFixedString);
            REGISTER2("addVal", add<Handle>, 2, void, HandleT, HandleT);

            REGISTER2("replaceFltAtIndex", replaceItemAtIndex<Float32>, 3, void, HandleT, Index, Float32);
            REGISTER2("replaceIntAtIndex", replaceItemAtIndex<SInt32>, 3, void, HandleT, Index, SInt32);
            REGISTER2("replaceStrAtIndex", replaceItemAtIndex<BSFixedString>, 3, void, HandleT, Index, BSFixedString);
            REGISTER2("replaceValAtIndex", replaceItemAtIndex<Handle>, 3, void, HandleT, Index, HandleT);

            REGISTER2("fltAtIndex", itemAtIndex<Float32>, 2, Float32, HandleT, Index);
            REGISTER2("intAtIndex", itemAtIndex<SInt32>, 2, SInt32, HandleT, Index);
            REGISTER2("strAtIndex", itemAtIndex<BSFixedString>, 2, BSFixedString, HandleT, Index);
            REGISTER2("valAtIndex", itemAtIndex<Handle>, 2, HandleT, HandleT, Index);

            REGISTER2("clear", clear, 1, void, HandleT);

            MESSAGE("funcs registered");

            return true;
        }
    }


    namespace tes_map {
        using namespace tes_object;

        static const char * TesName() { return "JMap";}

        static map* find(HandleT handle) {
            return collection_registry::getObjectOfType<map>(handle);
        }

        template<class T>
        static typename Tes2Value<T>::tes_type getItem(StaticFunctionTag*, HandleT handle, BSFixedString key) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj || key.data == nullptr) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            auto itr = obj->cnt.find(key.data);
            return itr != obj->cnt.end() ? itr->second.readAs<T>() : 0;
        }

        template<class T>
        static void setItem(StaticFunctionTag*, HandleT handle, BSFixedString key, typename Tes2Value<T>::tes_type item) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj || key.data == nullptr) {
                return;
            }

            mutex_lock g(obj->_mutex);
            obj->cnt[key.data] = Item((T)item);
        }

        static bool hasKey(StaticFunctionTag*, HandleT handle, BSFixedString key) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj || key.data == nullptr) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            auto itr = obj->cnt.find(key.data);
            return itr != obj->cnt.end();
        }

        static bool removeKey(StaticFunctionTag*, HandleT handle, BSFixedString key) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj || key.data == nullptr) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            auto itr = obj->cnt.find(key.data);
            bool hasKey = itr != obj->cnt.end();

            obj->cnt.erase(itr);
            return hasKey;
        }

        static SInt32 count(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            return obj->cnt.size();
        }

        static void clear(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            auto obj = find(handle);
            if (!obj) {
                return;
            }

            mutex_lock g(obj->_mutex);
            obj->cnt.clear();
        }

        bool registerFuncs(VMClassRegistry* registry) {

            REGISTER2("create", create<array>, 0, HandleT);
            REGISTER2("object", object<array>, 0, HandleT);

            REGISTER(count, 1, SInt32, HandleT);
            REGISTER2("clear", clear, 1, void, HandleT);
            REGISTER(removeKey, 2, bool, HandleT, BSFixedString);

            REGISTER2("setFlt", setItem<Float32>, 3, void, HandleT, BSFixedString, Float32);
            REGISTER2("setInt", setItem<SInt32>, 3, void, HandleT, BSFixedString, SInt32);
            REGISTER2("setStr", setItem<BSFixedString>, 3, void, HandleT, BSFixedString, BSFixedString);
            REGISTER2("setVal", setItem<Handle>, 3, void, HandleT, BSFixedString, HandleT);

            REGISTER2("getFlt", getItem<Float32>, 2, Float32, HandleT, BSFixedString);
            REGISTER2("getInt", getItem<SInt32>, 2, SInt32, HandleT, BSFixedString);
            REGISTER2("getStr", getItem<BSFixedString>, 2, BSFixedString, HandleT, BSFixedString);
            REGISTER2("getVal", getItem<Handle>, 2, HandleT, HandleT, BSFixedString);

            return true;
        }
    }
}