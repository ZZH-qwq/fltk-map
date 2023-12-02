#pragma once

#include "tilts.h"
#include "map_display.h"
#include "area_display.h"

namespace control{
    Fl_Double_Window* win;
    map::Fl_Map* m;
    area::Fl_Area* areas;

    // Print the size of an area
    class Fl_Area_Size_Output : public Fl_Widget {
    public:
        static const int lable_w = 6;
        Fl_Offscreen oscr;
        long double t_size = 0;
        bool legal = true;
        std::variant<area::Area*, std::list<area::Area>::iterator> t_area = nullptr;
        Fl_Area_Size_Output(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {
            oscr = fl_create_offscreen(w, h);
        }
        void draw() override {
            fl_begin_offscreen(oscr);
            fl_rectf(0, 0, w(), h(), FL_WHITE);
            fl_color(FL_BLACK);
            Fl_Fontsize t_h;
            switch (t_area.index()) {
            case 0: {
                auto a = std::get<0>(t_area);
                legal = a && a->size_legal();
                if (legal) {
                    t_size = a->temp_size();
                }
                t_h = h();
                fl_rectf(0, 0, lable_w, h(), a ? a->color() : FL_GRAY0);
                break;
            }
            default: {
                legal = true;
                const auto& a = std::get<1>(t_area);
                t_size = a->size();
                fl_font(0, static_cast<Fl_Fontsize>(h() / 2.5));
                t_h = h() / 2;
                fl_rectf(0, 0, lable_w, h(), a->color());
                fl_draw(a->name().c_str(), 0, static_cast<int>(h() / 1.7), w(), static_cast<int>(h() / 2.5), FL_ALIGN_RIGHT);
                break;
            }
            }
            fl_font(0, t_h);
            fl_color(FL_BLACK);
            if (legal) {
                std::stringstream ss;
                if (t_size > 1e9) {
                    t_size /= 1e6;
                    ss << std::scientific << std::setprecision(3) << t_size << " km ";
                } else if (t_size > 1e4) {
                    t_size /= 1e6;
                    ss << std::fixed << std::setprecision(4) << t_size << " km ";
                } else {
                    ss << std::setprecision(4) << t_size << " m ";
                }
#if DEBUG
                std::cout << ss.str() << std::endl;
#endif // DEBUG
                fl_draw(ss.str().c_str(), 0, 0, w(), t_h, FL_ALIGN_RIGHT);
                fl_font(1, static_cast<int>(t_h / 1.8));
                fl_draw("2", 0, 0, w(), static_cast<int>(t_h / 1.8), FL_ALIGN_RIGHT);
                //fl_draw((std::to_string(areas->temp->temp_size()) + " m^2").c_str(), x(), y(), w(), text_h, FL_ALIGN_RIGHT);
            } else {
                fl_draw("--- ", 0, 0, w(), t_h, FL_ALIGN_RIGHT);
            }
            fl_end_offscreen();
            fl_copy_offscreen(x(), y(), w(), h(), oscr, 0, 0);
        }
        ~Fl_Area_Size_Output() {
            fl_delete_offscreen(oscr);
        }
    };

    class Fl_Area_Info : public Fl_Group {
    public:
        static const int text_h = 60;
        Fl_Button* show_hide, * center_on;
        Fl_Area_Size_Output* size_op;
        std::list<area::Area>::iterator t_area;
        Fl_Area_Info(int w, int h) : Fl_Group(0, 0, w, h) {
            this->box(FL_DOWN_BOX);
            begin();
            color(FL_WHITE);
            auto p1 = new Fl_Pack(10, 10, w - 20, h - 20);
            p1->spacing(10);
            size_op = new Fl_Area_Size_Output(0, 0, w - 20, text_h);
            auto p2 = new Fl_Pack(0, 0, w, 30);
            p2->spacing(10);
            p2->type(FL_HORIZONTAL);
            show_hide = new Fl_Button(0, 0, w / 2 - 15, 30, "Hide");
            show_hide->callback(show_hide_cb, (void*)this);
            show_hide->color(FL_LIGHT3);
            center_on = new Fl_Button(0, 0, w / 2 - 15, 30, "Focus");
            center_on->callback(center_on_cb, (void*)this);
            center_on->color(FL_LIGHT3);
            p2->end();
            p1->end();
            end();
        }
        ~Fl_Area_Info() {
            delete show_hide;
            delete center_on;
            delete size_op;
        }
        void link(std::list<area::Area>::iterator a) {
            t_area = a;
            size_op->t_area = a;
        }

        static void show_hide_cb(Fl_Widget* o, void* v) {
            auto c = (Fl_Area_Info*)v;
            c->t_area->flip_visible();
            m->redraw_flag = true;
            ((Fl_Button*)o)->label((c->t_area->visible() ? "Hide" : "Show"));
            o->redraw();
            //win->redraw();
        }
        static void center_on_cb(Fl_Widget* o, void* v) {
            auto c = (Fl_Area_Info*)v;
            auto cen = c->t_area->center();
            m->focus_on(cen.x, cen.y);
            m->redraw_flag = true;
            //win->redraw();
        }
    };

    class Fl_Area_List : public Fl_Scroll {
    public:
        Fl_Pack* pack;
        Fl_Area_List(int x, int y, int w, int h) : Fl_Scroll(x, y, w, h) {
            this->type(Fl_Scroll::VERTICAL);
            this->color(FL_LIGHT3);
            this->scrollbar.color(FL_LIGHT3);
            this->scrollbar.color2(FL_LIGHT3);
            begin();
            int nbuttons = 24;
            pack = new Fl_Pack(x, y, 240, 285);
            pack->end();
            pack->spacing(10);
            pack->color(FL_WHITE);
            end();
        }
        ~Fl_Area_List() { delete pack; }
    }*area_list;

    class Fl_New_Area_Control : public Fl_Group {
    public:
        const int text_h = 30;
        size_t count = 0;
        Fl_Button* confirm_new, * undo, * new_area;
        Fl_Input* area_name;
        Fl_Area_Size_Output* size_op;
        Fl_New_Area_Control(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
            this->box(FL_DOWN_BOX);
            begin();
            color(FL_WHITE);
            confirm_new = new Fl_Button(x + 10, y + 150, w - 20, 30, "Confirm");
            confirm_new->callback(confirm_cb, (void*)this);
            undo = new Fl_Button(x + 10, y + 115, w - 20, 30, "Undo");
            undo->callback(undo_cb, (void*)this);
            area_name = new Fl_Input(x + 10, y + 60, w - 20, 30);
            area_name->value("SJTU");
            area_name->textsize(20);
            area_name->callback(rename_cb, (void*)this);
            area_name->when(FL_WHEN_CHANGED);
            size_op = new Fl_Area_Size_Output(x + 10, y + 15, w - 20, text_h);
            end();
            new_area = new Fl_Button(x + 10, y + 150, w - 20, 30, "New");
            new_area->callback(new_area_cb, (void*)this);
            new_area->hide();
        }

        void link() { size_op->t_area = areas->temp; }
        void update_size() { size_op->draw(); }
        void update_name() {
            std::string str = area_name->value();
            if (str.empty()) {
                str = "Area " + std::to_string(count);
                //area_name->value(str.c_str());
                //area_name->redraw();
            }
#if DEBUG
            std::cout << str << std::endl;
#endif // DEBUG
            areas->temp->set_name(str.c_str());
            win->redraw();
            update_size();
        }

        static void confirm_cb(Fl_Widget* o, void* v) {
            assert(areas->temp);
            if (areas->finish()) {
                auto info = new Fl_Area_Info(240, 120);
                area_list->pack->add(info);
                info->link(--(areas->areas.end()));
                area_list->redraw();
            }
            m->redraw_flag = 1;
            auto c = (Fl_New_Area_Control*)v;
            c->new_area->show();
            c->hide();
            c->new_area->take_focus();
        }
        static void new_area_cb(Fl_Widget* o, void* v) {
            auto [r, g, b] = area::color_generator();
            assert(!areas->temp);
            auto c = (Fl_New_Area_Control*)v;
            ++(c->count);
            areas->temp = new area::Area(m->Fl_Group::w(), m->Fl_Group::h(), r, g, b, 32);
            std::string str = "Area " + std::to_string(c->count);
            c->area_name->value(str.c_str());
            areas->temp->set_name(str);
            c->size_op->t_area = areas->temp;
            m->redraw_flag = true;
            c->show();
            c->redraw();
            c->update_size();
            c->take_focus();
            ((Fl_Button*)o)->hide();
        }
        static void undo_cb(Fl_Widget* o, void* v) {
            if (areas->temp) {
                areas->temp->undo_temp();
            }
            m->redraw_flag = true;
            //win->redraw();
            ((Fl_New_Area_Control*)v)->update_size();
        }
        static void rename_cb(Fl_Widget* o, void* v) {
            auto c = (Fl_New_Area_Control*)v;
            c->update_name();
        }
    }*new_area_control;
}
