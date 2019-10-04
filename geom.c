#include "/usr/local/include/raylib.h"
#include "/usr/local/include/raymath.h"
#include "../stb/stretchy_buffer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define float_tolerance 10e-6f

typedef struct option_t option_t;
struct option_t {
    Rectangle rec;
    char *str;
    bool active;
};

Vector2 *subdivide_none(Vector2 *points, size_t *amount, bool const closed) {
    return points;
}

Vector2 *bspline_quadratic(Vector2 *points, size_t *amount_in, bool const closed) {
    if (*amount_in == 1) return points;
    size_t amount_subdiv = closed ? *amount_in * 2 : *amount_in * 2 - 2;
    Vector2 *subdivided = malloc(amount_subdiv * sizeof *subdivided);

    for (size_t i = 0; i < *amount_in; ++i) {
        if (!closed && i == *amount_in - 1) break;
        subdivided[2*i] = Vector2Add(
                             Vector2Scale(points[i], 3.0/4.0),
                             Vector2Scale(points[(i+1)%*amount_in], 1.0/4.0));
        subdivided[2*i+1] = Vector2Add(
                               Vector2Scale(points[i], 1.0/4.0),
                               Vector2Scale(points[(i+1)%*amount_in], 3.0/4.0));
    }

    *amount_in = amount_subdiv;
    free(points);
    return subdivided;
}

Vector2 *bspline_cubic(Vector2 *points, size_t *amount_in, bool const closed) {
    if (*amount_in == 1) return points;
    size_t amount_subdiv = closed ? *amount_in * 2 : *amount_in * 2 - 3;
    Vector2 *subdivided = malloc(amount_subdiv * sizeof *subdivided);

    for (size_t i = 0; i < *amount_in; ++i) {
        subdivided[2*i] = Vector2Add(
                             Vector2Scale(points[i], 4.0/8.0),
                             Vector2Scale(points[(i+1)%*amount_in], 4.0/8.0));
        if (!closed && i == *amount_in - 2) break;
        subdivided[2*i+1] = Vector2Add(Vector2Add(
                               Vector2Scale(points[i], 1.0/8.0),
                               Vector2Scale(points[(i+1)%*amount_in], 6.0/8.0)),
                               Vector2Scale(points[(i+2)%*amount_in], 1.0/8.0));
    }

    *amount_in = amount_subdiv;
    free(points);
    return subdivided;
}

Vector2 *four_point_scheme(Vector2 *points, size_t *amount_in, bool const closed) {
    if (*amount_in < 4) return points;
    size_t amount_subdiv = closed ? *amount_in * 2 : *amount_in * 2 - 1;
    Vector2 *subdivided = malloc(amount_subdiv * sizeof *subdivided);

    for (size_t i = 0; i < *amount_in; ++i) {
        subdivided[2*i] = points[i];
        if (!closed && i > *amount_in - 3)
            continue;
        if (!closed && i == 0) {
            subdivided[(2*i+1)%amount_subdiv] = Vector2Add(Vector2Add(
                                Vector2Scale(points[i], 8.0/16.0),
                                Vector2Scale(points[(i+1)%*amount_in], 9.0/16.0)),
                                        Vector2Add(
                                Vector2Scale(points[(i+2)%*amount_in], -1.0/16.0),
                                Vector2Scale(points[(i+3)%*amount_in], 0.0)));
            subdivided[(2*i+3)%amount_subdiv] = Vector2Add(Vector2Add(
                                Vector2Scale(points[i], -1.0/16.0),
                                Vector2Scale(points[(i+1)%*amount_in], 9.0/16.0)),
                                        Vector2Add(
                                Vector2Scale(points[(i+2)%*amount_in], 9.0/16.0),
                                Vector2Scale(points[(i+3)%*amount_in], -1.0/16.0)));
            continue;
        }
        if (!closed && i == *amount_in - 3) {
            subdivided[(2*i+3)%amount_subdiv] = Vector2Add(Vector2Add(
                                Vector2Scale(points[i], -1.0/16.0),
                                Vector2Scale(points[(i+1)%*amount_in], 9.0/16.0)),
                                        Vector2Add(
                                Vector2Scale(points[(i+2)%*amount_in], 8.0/16.0),
                                Vector2Scale(points[(i+3)%*amount_in], 0.0)));
            continue;
        }
        subdivided[(2*i+3)%amount_subdiv] = Vector2Add(Vector2Add(
                              Vector2Scale(points[i], -1.0/16.0),
                              Vector2Scale(points[(i+1)%*amount_in], 9.0/16.0)),
                                       Vector2Add(
                              Vector2Scale(points[(i+2)%*amount_in], 9.0/16.0),
                              Vector2Scale(points[(i+3)%*amount_in], -1.0/16.0)));
    }

    *amount_in = amount_subdiv;
    free(points);
    return subdivided;
}

int main() {
    size_t const screen_width = 1600;
    size_t const screen_height = 900;
    InitWindow(screen_width, screen_height, "Subdivision Curves");

    SetTargetFPS(60);

    Vector2 mouse_pos;
    Vector2 *input_points = 0;
    Vector2 *subdivided_points = 0;
    option_t *options = 0;

    Vector2 *(*curr_subdivision_scheme)(Vector2 *, size_t *, bool) = subdivide_none;

    Font const font = GetFontDefault();
    double const font_size = 30.0;
    double const font_spacing = 1.0;

    size_t subdivided_amount = 0;
    double const overlap_epsilon = 8.0;
    bool overlap = 0;
    bool shape_closed = 0;
    bool drawing_finished = 0;

    Rectangle tmprec;
    Vector2 text_size;

    double max_text_size = 0.0;
    double const options_x_offset = 5.0;
    double const options_y_offset = 5.0;

#define AMNT_OPTIONS 9
    char *option_strs[AMNT_OPTIONS] = {"Toggle Points", "None", "Quadratic", "Cubic", "4-Point-Scheme", "New Drawing", "Depth: 5", "++ Depth", "-- Depth"};
    char *depth_counter_strs[9] = {"Depth: 1", "Depth: 2", "Depth: 3", "Depth: 4", "Depth: 5", "Depth: 6", "Depth: 7", "Depth: 8", "Depth: 9"};

    for (size_t i = 0; i < AMNT_OPTIONS; ++i) {
        option_t curr;
        curr.str = option_strs[i];
            text_size = MeasureTextEx(font, curr.str, font_size, font_spacing);
            tmprec.x = options_x_offset;
            tmprec.y = (i+1)*text_size.y + (i+1)*options_y_offset;
            tmprec.width = text_size.x;
            tmprec.height = text_size.y;
        if (text_size.x > max_text_size)
            max_text_size = text_size.x;
        curr.rec = tmprec;
        curr.active = 0;
        sb_push(options, curr);
    }
    options[1].active = 1;
    option_t *depth_counter_opt = &options[6];

    Rectangle show_points_rec = options[0].rec;
    Rectangle subdiv_none_rec = options[1].rec;
    Rectangle subdiv_bspl_quad_rec = options[2].rec;
    Rectangle subdiv_bspl_cube_rec = options[3].rec;
    Rectangle subdiv_4_points_rec = options[4].rec;
    Rectangle new_drawing_rec = options[5].rec;
    Rectangle depth_counter_rec = options[6].rec;
    Rectangle inc_depth_rec = options[7].rec;
    Rectangle dec_depth_rec = options[8].rec;

    Vector2 separator_line_start = {max_text_size + 2*options_x_offset, 0};
    Vector2 separator_line_end   = {max_text_size + 2*options_x_offset, screen_height};

    Rectangle drawing_boundary;
    drawing_boundary.x = max_text_size + 2*options_x_offset;
    drawing_boundary.y = 0.0;
    drawing_boundary.width = screen_width - max_text_size - 2*options_x_offset;
    drawing_boundary.height = screen_height;

    size_t display_points = 1;
    double const line_thickness = 2.0;
    size_t subdivision_depth = 5;
    Vector2 *held_point = 0;
    bool subdiv_changed = 0;

while (!WindowShouldClose()) { // Detect window close button or ESC key
    overlap = 0;
    subdiv_changed = 0;

    mouse_pos = GetMousePosition();

    if (sb_count(input_points)) if (fabs(mouse_pos.x - input_points[0].x) < overlap_epsilon && fabs(mouse_pos.y - input_points[0].y) < overlap_epsilon)
        overlap = 1;

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && held_point && CheckCollisionPointRec(mouse_pos, drawing_boundary)) {
        *held_point = mouse_pos;
        subdiv_changed = 1;
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && held_point)
        held_point = 0;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (drawing_finished) {
            for (size_t i = 0; i < sb_count(input_points); ++i)
                if (fabs(mouse_pos.x - input_points[i].x) < overlap_epsilon && fabs(mouse_pos.y - input_points[i].y) < overlap_epsilon)
                    held_point = &input_points[i];
        }
        if (CheckCollisionPointRec(mouse_pos, drawing_boundary) && !drawing_finished) {
            if (overlap) {
                shape_closed = 1;
                drawing_finished = 1;
                subdivided_amount = sb_count(input_points);
                subdivided_points = malloc(subdivided_amount * sizeof *subdivided_points);
                memcpy(subdivided_points, input_points, subdivided_amount * sizeof *subdivided_points);
                subdiv_changed = 1;
            } else {
                sb_push(input_points, mouse_pos);
            }
        } else if (CheckCollisionPointRec(mouse_pos, show_points_rec)) {
            display_points = !display_points;
        } else if (CheckCollisionPointRec(mouse_pos, subdiv_none_rec)) {
            curr_subdivision_scheme = subdivide_none;
            if (!drawing_finished && sb_count(input_points))
                drawing_finished = 1;
            for (size_t i = 0; i < sb_count(options); ++i)
                options[i].active = 0;
            options[1].active = 1;
            subdiv_changed = 1;
        } else if (CheckCollisionPointRec(mouse_pos, subdiv_bspl_quad_rec)) {
            curr_subdivision_scheme = bspline_quadratic;
            if (!drawing_finished && sb_count(input_points))
                drawing_finished = 1;
            for (size_t i = 0; i < sb_count(options); ++i)
                options[i].active = 0;
            options[2].active = 1;
            subdiv_changed = 1;
        } else if (CheckCollisionPointRec(mouse_pos, subdiv_bspl_cube_rec)) {
            curr_subdivision_scheme = bspline_cubic;
            if (!drawing_finished && sb_count(input_points))
                drawing_finished = 1;
            for (size_t i = 0; i < sb_count(options); ++i)
                options[i].active = 0;
            options[3].active = 1;
            subdiv_changed = 1;
        } else if (CheckCollisionPointRec(mouse_pos, subdiv_4_points_rec)) {
            curr_subdivision_scheme = four_point_scheme;
            if (!drawing_finished && sb_count(input_points))
                drawing_finished = 1;
            for (size_t i = 0; i < sb_count(options); ++i)
                options[i].active = 0;
            options[4].active = 1;
            subdiv_changed = 1;
        } else if (CheckCollisionPointRec(mouse_pos, new_drawing_rec)) {
            sb_free(input_points);
            input_points = 0;
            free(subdivided_points);
            subdivided_points = 0;
            subdivided_amount = 0;
            drawing_finished = 0;
            shape_closed = 0;
            curr_subdivision_scheme = subdivide_none;
            for (size_t i = 0; i < sb_count(options); ++i)
                options[i].active = 0;
            options[1].active = 1;
            subdiv_changed = 1;
        } else if (CheckCollisionPointRec(mouse_pos, inc_depth_rec)) {
            ++subdivision_depth;
            if (subdivision_depth > 9)
                subdivision_depth = 9;
            depth_counter_opt->str = depth_counter_strs[subdivision_depth - 1];
            subdiv_changed = 1;
        } else if (CheckCollisionPointRec(mouse_pos, dec_depth_rec)) {
            --subdivision_depth;
            if (subdivision_depth == 0)
                subdivision_depth = 1;
            depth_counter_opt->str = depth_counter_strs[subdivision_depth - 1];
            subdiv_changed = 1;
        }
    }

    if (drawing_finished && subdiv_changed) {
        if (subdivided_points) free(subdivided_points);
        subdivided_amount = sb_count(input_points);
        subdivided_points = malloc(subdivided_amount * sizeof *subdivided_points);
        memcpy(subdivided_points, input_points, subdivided_amount * sizeof *subdivided_points);
        for (size_t i = 0; i < subdivision_depth; ++i)
            subdivided_points = curr_subdivision_scheme(subdivided_points, &subdivided_amount, shape_closed);
    }

    BeginDrawing();
        ClearBackground(RAYWHITE);

        // draw options
        for (size_t i = 0; i < sb_count(options); ++i) {
            if (options[i].active)
                DrawRectangleRec(options[i].rec, SKYBLUE);
            DrawTextEx(font, options[i].str, (Vector2){options[i].rec.x, options[i].rec.y}, font_size, font_spacing, BLACK);
            if (CheckCollisionPointRec(mouse_pos, options[i].rec))
                DrawRectangleLinesEx(options[i].rec, 1, BLACK);
        }

        DrawLineV(separator_line_start, separator_line_end, BLACK);

        // draw input polygon
        for (size_t i = 0; i < sb_count(input_points); ++i) {
            Color color = drawing_finished ? GRAY : BLACK;
            if (display_points || drawing_finished)
                DrawCircleV(input_points[i], 3, color);

            if (i < sb_count(input_points) - 1) {
                DrawLineEx(input_points[i], input_points[i+1], line_thickness, color);
            } else if (!drawing_finished) {
                if (CheckCollisionPointRec(mouse_pos, drawing_boundary)) {
                    if (overlap)
                        DrawLineEx(input_points[i], input_points[0], line_thickness, BLUE);
                    else
                        DrawLineEx(input_points[i], mouse_pos, line_thickness, BLUE);
                }
            } else {
                if (shape_closed)
                    DrawLineEx(input_points[i], input_points[0], line_thickness, color);
            }
        }

        // draw subdivided polygon
        for (size_t i = 0; i < subdivided_amount && drawing_finished; ++i) {
            if (display_points)
                DrawCircleV(subdivided_points[i], 3, BLACK);
            if (i < subdivided_amount - 1)
                DrawLineEx(subdivided_points[i], subdivided_points[i+1], line_thickness, BLACK);
            else
                if (shape_closed)
                    DrawLineEx(subdivided_points[i], subdivided_points[0], line_thickness, BLACK);
        }
        
        // draw mouse circle
        if (!drawing_finished && !overlap && CheckCollisionPointRec(mouse_pos, drawing_boundary))
            DrawCircleLines(mouse_pos.x, mouse_pos.y, 3, BLUE);
    EndDrawing();
} // end program while loop
    
    CloseWindow();
    return 0;
}
