#pragma once

namespace collections {

    struct JCFixture : testing::Fixture {
        tes_context context;
    };

#   define JC_TEST(name, name2) TEST_F(JCFixture, name, name2)
#   define JC_TEST_DISABLED(name, name2) TEST_F(JCFixture, name, DISABLED_##name2)

}

namespace collections { namespace {

    JC_TEST(object_base, refCount)
    {
        auto obj = &array::object(context);
        EXPECT_TRUE(obj->refCount() == 0); // aqueue retains it -- no more

        obj->retain();
        EXPECT_TRUE(obj->refCount() == 1);

        obj->tes_retain();
        obj->tes_retain();
        obj->tes_retain();
        EXPECT_TRUE(obj->refCount() == 1 + 3);

        // ensure that over-release does not affects internal ref count:
        for (int i = 0; i < 20; i++) {
            obj->tes_release();
        }
        EXPECT_TRUE(obj->refCount() == 1);

        obj->release();
        EXPECT_TRUE(obj->refCount() == 1); // aqueue retains it

        // that will damage memory, later: 
        //obj->release();
        //EXPECT_TRUE(obj->refCount() == 1);
    }


    JC_TEST(Item, nulls)
    {
        Item i1;

        EXPECT_TRUE(i1.isNull());

        i1 = (const char*)nullptr;
        EXPECT_TRUE(i1.isNull());

        i1 = (TESForm*)nullptr;
        EXPECT_TRUE(i1.isNull());

        i1 = FormZero;
        EXPECT_TRUE(i1.isNull());

        i1 = (object_base *)nullptr;
        EXPECT_TRUE(i1.isNull());
    }

    JC_TEST(Item, equality)
    {
        Item i1, i2;

        EXPECT_TRUE(i1.isNull());
        EXPECT_TRUE(i2.isNull());

        EXPECT_TRUE(i1.isEqual(i1));
        EXPECT_TRUE(i1.isEqual(i2));

        i1 = "me";
        i2 = "me";
        EXPECT_TRUE(i1.isEqual(i2));

        i2 = "not me";
        EXPECT_FALSE(i1.isEqual(i2));

        i1 = 1u;
        i2 = 1u;
        EXPECT_TRUE(i1.isEqual(i2));

        i2 = 1.5f;
        EXPECT_FALSE(i1.isEqual(i2));

        auto& obj = array::object(context);
        i1 = obj;
        i2 = obj;
        EXPECT_TRUE(i1.isEqual(i2));
    }

    JC_TEST(Item, less_than)
    {
        EXPECT_TRUE(Item(100) < Item(2.0));
        EXPECT_TRUE(Item(1.0) < Item(2.0));
        EXPECT_TRUE(Item(10) < Item(FormZero));
        EXPECT_TRUE(Item("aa") < Item("text"));
        //EXPECT_TRUE(Item(1.0) < Item(FormZero));
    }

    TEST(form_handling, test)
    {
        namespace fh = form_handling;

        EXPECT_TRUE(fh::is_form_string("__formData|Skyrim.esm|0x1"));
        EXPECT_FALSE(fh::is_form_string("__formDatttt"));
        EXPECT_FALSE(fh::is_form_string(nullptr));

        // test static form ids
        {
            const int pluginIdx = 'B';
            const FormId form = (FormId)(pluginIdx << 24 | 0x14);

            EXPECT_TRUE(fh::is_static(form));
            EXPECT_EQ(form, fh::construct(pluginIdx, 0x14));

            std::string formString = *fh::to_string(form);
            EXPECT_TRUE(formString ==
                (std::string(fh::kFormData) + fh::kFormDataSeparator + skse::modname_from_index(pluginIdx) + fh::kFormDataSeparator + "0x14"));

            EXPECT_TRUE(form ==
                *fh::from_string(formString.c_str()));

        }

        // test global (0xFF*) form ids
        {
            const FormId form = (FormId)(FormGlobalPrefix << 24 | 0x14);

            EXPECT_TRUE(!fh::is_static(form));
            EXPECT_EQ(form, fh::construct(FormGlobalPrefix, 0x14));

            std::string formString = *fh::to_string(form);

            EXPECT_TRUE(formString ==
                (std::string(fh::kFormData) + fh::kFormDataSeparator + fh::kFormDataSeparator + "0xff000014"));

            EXPECT_TRUE(form ==
                *fh::from_string(formString.c_str()));
        }
        {
            const char *unresolveableFString = "__formData|ssa.esm|0x1";

            EXPECT_TRUE(fh::is_form_string(unresolveableFString));
            EXPECT_FALSE(fh::from_string(unresolveableFString));

            // is invalid in sythetic test only: all plugin indexes except 'A'-'Z' are invalid
            FormId invalidFormId = (FormId)fh::construct('%', 0x14);
            EXPECT_FALSE(fh::to_string(invalidFormId));
        }
    }

    TEST(reference_serialization, test) {

        const char* testData[][2] = {
            "__reference|", "",
            "__reference|anyString", "anyString",
            "__reference||anyString", "|anyString",

            "__reference", nullptr,
            nullptr, nullptr,
            "", nullptr,
            "__", nullptr,
        };

        auto pathExtract = [&](const char* refString, const char* path) {
            auto res = reference_serialization::extract_path(refString);
            EXPECT_TRUE((!res && !path) || strcmp(res, path) == 0);
        };

        for (auto& row : testData) {
            pathExtract(row[0], row[1]);
        }
    }

    JC_TEST(json_deserializer, test)
    {
        EXPECT_NIL(json_deserializer::object_from_file(context, ""));
        EXPECT_NIL(json_deserializer::object_from_file(context, nullptr));

        EXPECT_NIL(json_deserializer::object_from_json_data(context, ""));
        EXPECT_NIL(json_deserializer::object_from_json_data(context, nullptr));
    }

    // load json file into tes_context -> serialize into json again -> compare with original json
    // also compares original json with json, loaded from serialized tex_context (do_comparison2 function)
    struct json_loading_test : testing::Fixture {

        void test() {

            namespace fs = boost::filesystem;

            auto dir = util::relative_to_dll_path("test_data/json_loading_test");
            fs::directory_iterator end;
            bool atLeastOneTested = false;

            for (fs::directory_iterator itr(dir); itr != end; ++itr) {
                if (fs::is_regular_file(*itr)) {
                    atLeastOneTested = true;
                    do_comparison(itr->path().generic_string().c_str());
                    do_comparison2(itr->path().generic_string().c_str());
                }
            }

            EXPECT_TRUE(atLeastOneTested);
        }

        void do_comparison(const char *file_path) {
            EXPECT_NOT_NIL(file_path);

            tes_context ctx;

            auto root = json_deserializer::object_from_file(ctx, file_path);
            EXPECT_NOT_NIL(root);
            auto jsonOut = json_serializer::create_json_value(*root);
            ctx.clearState();

            auto originJson = json_deserializer::json_from_file(file_path);
            EXPECT_NOT_NIL(originJson);

            EXPECT_TRUE(json_equal(originJson.get(), jsonOut.get()) == 1);
        }

        void do_comparison2(const char *file_path) {
            EXPECT_NOT_NIL(file_path);

            auto jsonOut = make_unique_ptr((json_t*)nullptr, &json_decref);
            {
                tes_context ctx;

                Handle rootId = HandleNull;
                {
                    auto root = json_deserializer::object_from_file(ctx, file_path);
                    EXPECT_NOT_NIL(root);
                    rootId = root->uid();
                }

                auto state = ctx.write_to_string();
                ctx.clearState();

                ctx.read_from_string(state, serialization_version::current);

                jsonOut = json_serializer::create_json_value(*ctx.getObject(rootId));
                ctx.clearState();
            }

            auto originJson = json_deserializer::json_from_file(file_path);
            EXPECT_NOT_NIL(originJson);

            EXPECT_TRUE(json_equal(originJson.get(), jsonOut.get()) == 1);
        }
    };

    TEST_F_CUSTOM_CLASS(json_loading_test, t);

    JC_TEST(json_serializer, no_infinite_recursion)
    {
        {
            map& cnt = map::object(context);
            cnt.u_setValueForKey("cycle", Item(cnt));

            json_serializer::create_json_data(cnt);
        }
        {
            map &cnt1 = map::object(context);
            map &cnt2 = map::object(context);

            cnt1.u_setValueForKey("cnt2", Item(cnt2));
            cnt2.u_setValueForKey("cnt1", Item(cnt1));

            json_serializer::create_json_data(cnt1);
        }
    }

    JC_TEST(json_handling, object_references)
    {
        object_base* root = json_deserializer::object_from_json_data(context, STR(
        {
            "parentArray": [
            {
                "objChildArrayOfChildJMap1": [],
                    "rootRef" : "__reference|"
            },
            {
                "objChildArrayOfChildJMap2": [],
                "referenceToChildJMap1" : "__reference|.parentArray[0]",
                "referenceToFormMapValue" : "__reference|.parentArray[2][__formData|D|0x4]"
            },
            {
                "__formData": null,
                "__formData|D|0x4" : []
            }
            ]
        }
        ));

        auto compareRefs = [&](object_base *root, const char *path1, const char *path2) {
            auto o1 = path_resolving::_resolve<object_base*>(context, root, path1);
            auto o2 = path_resolving::_resolve<object_base*>(context, root, path2);
            EXPECT_TRUE(o1 && o2 && o1 == o2);
        };

        auto validateGraph = [&](object_base *root) {

            EXPECT_NOT_NIL(root);

            const char *equalPaths[][2] = {
                ".parentArray[0]", ".parentArray[1].referenceToChildJMap1",
                ".parentArray[0]", ".parentArray[1].referenceToChildJMap1",
                ".parentArray[0].rootRef", "",
                ".parentArray[2][__formData|D|0x4]", ".parentArray[1].referenceToFormMapValue"
            };

            for (auto& pair : equalPaths) {
                compareRefs(root, pair[0], pair[1]);
            }
        };


        validateGraph(root);

        auto jvalue = json_serializer::create_json_value(*root);
        auto root2 = json_deserializer::object_from_json(context, jvalue.get());
        validateGraph(root2);
    }

    JC_TEST(tes_context, backward_compatibility)
    {
        namespace fs = boost::filesystem;

        fs::path dir = util::relative_to_dll_path("test_data/backward_compatibility");
        fs::directory_iterator end;
        bool atLeastOneTested = false;

        for (fs::directory_iterator itr(dir); itr != end; ++itr) {
            if (fs::is_regular_file(*itr)) {
                atLeastOneTested = true;

                std::ifstream file(itr->path().generic_string(), std::ios::in | std::ios::binary);
                // had to pass kJSerializationNoHeaderVersion - 0.67 has no header :(
                context.read_from_stream(file, serialization_version::no_header);
            }
        }

        EXPECT_TRUE(atLeastOneTested);
    }

    JC_TEST(deep_copying, _)
    {
        {
            // array containing himself
            array& root = json_deserializer::object_from_json_data(context, STR(
                ["__reference|"]
            ))->as_link<array>();
            //EXPECT_NOT_NIL(root);

            array& copy = deep_copying::deep_copy(context, root).as_link<array>();
            EXPECT_TRUE(&copy != &root);

            EXPECT_TRUE(copy[0] == copy);
            EXPECT_TRUE(root[0] == root);

            EXPECT_TRUE(root.s_count() == 1);
        }
        {
            auto& root = json_deserializer::object_from_json_data(context, STR(
                { "c": "__reference|.b", "b": [] }
            ))->as_link<map>();

            auto& copy = deep_copying::deep_copy(context, root).as_link<map>();
            EXPECT_TRUE(&copy != &root);
            EXPECT_TRUE(root.s_count() == 2);
            EXPECT_TRUE(copy.s_count() == 2);

            EXPECT_TRUE(copy["c"] == copy["b"]);
            EXPECT_TRUE(root["c"] == root["b"]);

            EXPECT_TRUE(root["c"] != copy["c"]);
            EXPECT_TRUE(root["b"] != copy["b"]);
        }
    }

    JC_TEST(garbage_collection, circular_references)
    {
        EXPECT_TRUE(context.collect_garbage() == 0);

        std::vector<array*> arrays;
        std::generate_n(std::back_inserter(arrays), 20, [&]{ return &array::object(context); });

        // randonly connect half of them
        for (int i = 0; i < 10; ++i) {
            auto rnd = rand() % 10;
            auto cont = arrays[rnd];

            for (int j = 0; j < 10; ++j) {
                auto rnd = rand() % 10;

                cont->push(Item(arrays[rnd]));
            }
        }

        EXPECT_TRUE(std::any_of(arrays.begin(), arrays.end(), [](array* ar) { return !ar->noOwners(); }));

        EXPECT_TRUE(context.collect_garbage() == arrays.size());
        EXPECT_TRUE(context.collect_garbage() == 0);
    }
}
}

namespace collections { namespace {

    JC_TEST(map, key_case_insensitivity)
    {
        map &cnt = map::object(context);

        std::string name = "back in black";
        cnt.u_setValueForKey("ACDC", Item(name));

        EXPECT_TRUE(*cnt.u_find("acdc")->stringValue() == name);
    }

    JC_TEST(tes_context, database)
    {
        auto db = context.database();
        EXPECT_TRUE(db != nullptr);
        EXPECT_TRUE(db == context.database());
    }

    JC_TEST(tes_context, database_setter)
    {
        auto root = &map::object(context);
        auto rc1 = root->refCount();
        context.setDataBase(root);
        const auto rcDiff = root->refCount() - rc1;
        EXPECT_TRUE(rcDiff > 0);

        auto rc2 = root->refCount();
        context.setDataBase(nullptr);
        const auto rcDiff2 = - root->refCount() + rc2;
        // had to hardcode this - old. db gets released, gets retained by aqueue, thus -rcDiff2 != rcDiff
        EXPECT_TRUE((1 + rcDiff2) == rcDiff);
    }

    JC_TEST(autorelease_queue, over_release)
    {
        std::vector<Handle> identifiers;
        //int countBefore = queue.count();

        for (int i = 0; i < 10; ++i) {
            auto obj = &map::make(context);//rc is 0
            identifiers.push_back(obj->public_id());

            EXPECT_TRUE(obj->refCount() == 0);

            obj->retain();//+1 rc
            EXPECT_TRUE(obj->refCount() == 1);

            obj->release();//-1, rc is 0, add to aqueue, rc is 1
            EXPECT_TRUE(obj->refCount() == 1); // queue owns it now

            obj->retain();// +1, rc is 2
            EXPECT_TRUE(obj->refCount() == 2);
        }

        auto allExist = [&]() {
            return std::all_of(identifiers.begin(), identifiers.end(), [&](Handle id) {
                return context.getObject(id);
            });
        };

        std::this_thread::sleep_for(std::chrono::seconds(14));

        //
        EXPECT_TRUE(allExist());

        for (Handle id : identifiers) {
            auto obj = context.getObject(id);
            EXPECT_TRUE(obj->refCount() == 1);
        }
    }

    JC_TEST(autorelease_queue, ensure_destroys)
    {
        std::vector<Handle> public_identifiers, privateIds;

        for (int i = 0; i < 10; ++i) {
            auto obj = &map::object(context);
            public_identifiers.push_back(obj->uid());

            auto priv = &map::make(context);
            // function order below makes assumption about internal impl. which is bad:
            priv->prolong_lifetime();
            privateIds.push_back(priv->public_id());
        }

        auto allExist = [&](std::vector<Handle>& identifiers) {
            size_t cntEx = 0;
            for (auto id : identifiers) {
                cntEx += (context.getObject(id) ? 1 : 0);
            }
            jc_debug("%u exist of %u", cntEx, identifiers.size());
            return cntEx == identifiers.size();
        };

        auto allDestroyed = [&](std::vector<Handle>& identifiers) {
            size_t cntDestr = 0;
            for (auto id : identifiers) {
                cntDestr += (context.getObject(id) ? 0 : 1);
            }
            jc_debug("%u destroyed of %u", cntDestr, identifiers.size());
            return cntDestr == identifiers.size();
        };

        std::this_thread::sleep_for(std::chrono::seconds(5));
        EXPECT_TRUE(allDestroyed(privateIds));
        EXPECT_TRUE(allExist(public_identifiers));

        std::this_thread::sleep_for(std::chrono::seconds(9));
        EXPECT_TRUE(allDestroyed(public_identifiers));
        EXPECT_TRUE(allDestroyed(privateIds));
    }

    JC_TEST(deadlock, _)
    {
        auto& obj = map::object(context);
        object_lock lock(obj);
        obj.u_setValueForKey("lol", Item(obj));
    }

}
}