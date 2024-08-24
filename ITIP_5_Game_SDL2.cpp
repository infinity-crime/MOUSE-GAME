#include <iostream>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#define MAX_COUNT 50 // максимальное кол-во стенок в лабиринте

/* глобальная область переменных */

int window_width = 1250; // первоначальная ширина окна
int window_height = 700; // первоначальная высота окна

SDL_Window* window = nullptr; // указатель на созданное окно
SDL_Renderer* render = nullptr; // указатель на рендер

int frame = 0; // текущий кадр
int count_frame = 4; // кол-во кадров для анимации мышки

int kursor_x = window_width / 100; // начальная позиция курсора (предпологаемая) по оси х
int kursor_y = window_height / 2; // начальная позиция курсора (предпологаемая) по оси у

int mouse_x = window_width / 100; // начальная позиция мышки по оси х
int mouse_y = window_height / 2; // начальная позиция мышки по оси у

int speed = 190; // скорость бега мышки за курсором
int FPS = 60; // кол-во кадров в секунду

/* конец глобальной области */

class ObjectTexture // класс для создания текстур
{
public:
	SDL_Rect src; // соурсник
	SDL_Rect dst; // размеры отображения в окне
	SDL_Texture* texture = nullptr; // указатель на созданную текстуру

	ObjectTexture() {}
	~ObjectTexture()
	{
		if (texture) // освобождаем память для той текстуры, которая сохранилась в классе
		{
			std::cout << "Destructor ObjectTexture" << std::endl;
			SDL_DestroyTexture(texture);
		}
	}

	SDL_Texture* create_texture(const char* filename) // метод создания текстуры
	{
		SDL_Surface* surface = IMG_Load(filename);
		if (surface == nullptr)
		{
			std::cout << "Error IMG_Load(): " << IMG_GetError() << std::endl;
			return nullptr;
		}

	    texture = SDL_CreateTextureFromSurface(render, surface);
		if (texture == nullptr)
		{
			std::cout << "Error SDL_CreateTextureFromSurface(): " << SDL_GetError() << std::endl;
			SDL_FreeSurface(surface);
			return nullptr;
		}

		src = { 0, 0, surface->w, surface->h };
		dst = { 0, 0, 0, 0 };

		SDL_FreeSurface(surface);
		return texture;
	}

	void set_src(int x, int y, int w, int h)
	{
		src.x = x;
		src.y = y;
		src.w = w;
		src.h = h;
	}

	void set_dst(int x, int y, int w, int h)
	{
		dst.x = x;
		dst.y = y;
		dst.w = w;
		dst.h = h;
	}
};

class FontObject // класс для объектов шрифта
{
	SDL_Surface* surface = nullptr;
	SDL_Texture* texture = nullptr;
	TTF_Font* font = nullptr;
public:
	SDL_Rect font_dst = { 0, 0, 0, 0 };

	FontObject() {}
	~FontObject() // освобождаем текстуру со шрифтом и сам шрифт
	{
		std::cout << "Destructor FontObject" << std::endl;
		SDL_DestroyTexture(texture);
		TTF_CloseFont(font);
	}

	void load_font(const char* filename, int size) // метод загрузки шрифта по имени файла и задание размера
	{
		font = TTF_OpenFont(filename, size);
		if (font == nullptr)
		{
			std::cout << "Error open font: " << TTF_GetError() << std::endl;
		}
	}

	SDL_Texture* render_text(const char* text) // метод для получения текстуры со шрифтом
	{
		if (font == nullptr)
		{
			return nullptr;
		}

		surface = TTF_RenderText_Blended(font, text, { 0, 0, 0, 255 });
		texture = SDL_CreateTextureFromSurface(render, surface);

		font_dst.w = surface->w;
		font_dst.h = surface->h;
		SDL_FreeSurface(surface);

		return texture;
	}
};

void Init_SDL2(Uint32 flags) // инициализация библиотеки
{
	// подключение SDL2
	if (SDL_Init(flags) != 0)
	{
		std::cout << "Error SDL_Init(): " << SDL_GetError() << std::endl;
		exit(1);
	}

	// подключение SDL_Image для работы с png и jpg
	if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0)
	{
		std::cout << "Error IMG_Init(): " << SDL_GetError() << std::endl;
		SDL_Quit();
		exit(1);
	}

	// подключение SDL_TTF для использования шрифтов
	if (TTF_Init() != 0)
	{
		std::cout << "Error TTF_Init(): " << SDL_GetError() << std::endl;
		SDL_Quit();
		IMG_Quit();
		exit(1);
	}

	// создание окна
	window = SDL_CreateWindow("Mouse Miki Game", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window == nullptr)
	{
		std::cout << "Error SDL_CreateWindow(): " << SDL_GetError() << std::endl;
		IMG_Quit();
		SDL_Quit();
		exit(1);
	}

	// создание рендера
	render = SDL_CreateRenderer(window, -1, 0);
	if (render == nullptr)
	{
		std::cout << "Error SDL_CreateRenderer(): " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(window);
		IMG_Quit();
		SDL_Quit();
		exit(1);
	}
}

// функция, которая изменяет положение мышки в пространстве, следуя за курсором (побочный эффект)
float move_mouse(int& mouse_x, int& mouse_y, int& kursor_x, int& kursor_y, int FPS, int speed)
{
	float x = kursor_x - mouse_x;
	float y = kursor_y - mouse_y;
	float len = sqrt(x * x + y * y);

	if (len == 0)
		len = 0.1;

	x /= len;
	y /= len;

	if (len > 30)
	{
		mouse_x += x * speed / FPS;
		mouse_y += y * speed / FPS;
	}
	return len;
}

// функция отрисовки стенок лабиринта и заполнения координатами четырех массивов для дальнейшего прохода
int drawing_maze(int* wall_x, int* wall_y, int* wall_w, int* wall_h)
{
	int counter = 0;
	SDL_SetRenderDrawColor(render, 0, 0, 0, 255);

	SDL_Rect rect = {90, 80, 10, window_height / 1.4 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = {90, 80, 200, 10};
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = {170, 0, 10, 80};
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = {390, 80, 775, 10};
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 1165, 80, 10, 200 }; // 5
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 1165, 340, 10, 300 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 1165, 490, 85, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 90, 640, 1085, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 170, 150, 10, 430 }; // 9
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 170, 150, 300, 10 }; 
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 570, 150, 520, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 1090, 150, 10, 190 }; // 12
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 1090, 400, 10, 180 }; 
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 170, 580, 930, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 90, 400, 80, 10 }; // 15
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 770, 80, 10, 70 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 250, 230, 10, 270 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 250, 230, 220, 10 }; // 18
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 250, 500, 110, 10 }; 
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 470, 230, 10, 100 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 390, 330, 90, 10 }; // 21
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 390, 330, 10, 90 }; 
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 390, 420, 500, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 740, 230, 10, 190 }; // 24
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 740, 230, 260, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 1000, 230, 10, 270 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 450, 500, 560, 10 }; // 27
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 700, 500, 10, 80 }; 
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;

	rect = { 830, 310, 170, 10 };
	SDL_RenderFillRect(render, &rect);
	wall_x[counter] = rect.x;
	wall_y[counter] = rect.y;
	wall_w[counter] = rect.w;
	wall_h[counter] = rect.h;
	counter++;
	
	return counter;
}

// проверка столкновения координат мышки с координатами стенки
bool check_collision_wall(int mouse_x, int mouse_y,int mouse_w, int mouse_h, int* wall_x, int* wall_y, int* wall_w, int* wall_h, int counter)
{
	for (int i = 0; i < counter; ++i)
	{
		if ((mouse_x + (mouse_w / 2) < wall_x[i] + wall_w[i]) &&
			(mouse_x + (mouse_w / 2) > wall_x[i]) &&
			(mouse_y + (mouse_h / 2) < wall_y[i] + wall_h[i]) &&
			(mouse_y + (mouse_h / 2) > wall_y[i]))
		{
			std::cout << "Mouse!!!: " << mouse_x << ' ' << mouse_y << std::endl;
			std::cout << "Wall!!!: " << wall_x[counter - 1] << ' ' << wall_h[counter - 1] << std::endl;
			return true;
		}
	}
	return false;
}

// проверка столкновения мышки с ключиком (случай выигрыша)
bool check_collisoin_key(int mouse_x, int mouse_y, int mouse_w, int mouse_h, int key_x, int key_y)
{
	if ((mouse_x + (mouse_w / 2) < key_x + 50) && (mouse_x + (mouse_w / 2) > key_x) 
		&& (mouse_y + (mouse_h / 2) < key_y + 50) && (mouse_y + (mouse_h / 2) > key_y))
		return true;
	return false;
}

// функция для игрового меню
bool game_menu(SDL_Texture* background, SDL_Rect src, SDL_Rect dst)
{
	unsigned short choise = 0; // start - 1, options - 0, exit - 2

#pragma region button_textures
	ObjectTexture button_start_object;
	SDL_Texture* button_start_texture = button_start_object.create_texture("button_start.png");
	button_start_object.set_src(0, 0, button_start_object.src.w / 2, button_start_object.src.h);
	button_start_object.set_dst(500, 300, 240, 80);

	ObjectTexture button_options_object;
	SDL_Texture* button_options_texture = button_options_object.create_texture("button_options.png");
	button_options_object.set_src(0, 0, button_options_object.src.w / 2, button_options_object.src.h);
	button_options_object.set_dst(500, 410, 240, 80);

	ObjectTexture button_exit_object;
	SDL_Texture* button_exit_texture = button_exit_object.create_texture("button_exit.png");
	button_exit_object.set_src(0, 0, button_exit_object.src.w / 2, button_exit_object.src.h);
	button_exit_object.set_dst(500, 520, 240, 80);
#pragma endregion button_textures

#pragma region load_font
	FontObject font_object;
	font_object.load_font("RAVIE.TTF", 18);
	SDL_Texture* text_1 = font_object.render_text("The author of this project is Kirill Zhestkov, studying in group O734B.");
	font_object.font_dst.x = 5;


	FontObject font_object_1;
	font_object_1.load_font("RAVIE.TTF", 18);
	SDL_Texture* text_2 = font_object_1.render_text("The essence of the game: reach the key in the center of the labyrinth, controlling the character.");
	font_object_1.font_dst.x = 5;
	font_object_1.font_dst.y = 100;

	FontObject font_object_2;
	font_object_2.load_font("RAVIE.TTF", 18);
	SDL_Texture* text_3 = font_object_2.render_text("Controls: the character runs after the cursor, so that it starts to follow, you need to hold.");
	font_object_2.font_dst.x = 5;
	font_object_2.font_dst.y = 200;

	FontObject font_object_3;
	font_object_3.load_font("RAVIE.TTF", 18);
	SDL_Texture* text_4 = font_object_3.render_text("That's all! Good luck!");
	font_object_3.font_dst.x = 5;
	font_object_3.font_dst.y = 300;

	if (text_1 == nullptr)
	{
		std::cout << "Error font_texture: " << SDL_GetError() << std::endl;
		SDL_DestroyTexture(button_start_texture);
		SDL_DestroyTexture(button_exit_texture);
		SDL_DestroyTexture(button_options_texture);
		return false;
	}

#pragma endregion load_font

	SDL_Event ev;
	bool is_running = true;
	bool is_options = false;

	while (is_running)
	{
		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				SDL_DestroyTexture(button_start_texture);
				SDL_DestroyTexture(button_exit_texture);
				SDL_DestroyTexture(button_options_texture);

				SDL_DestroyTexture(text_1);
				SDL_DestroyTexture(text_2);
				SDL_DestroyTexture(text_3);
				SDL_DestroyTexture(text_4);
				is_running = false;
				return false;
				break;

			case SDL_KEYDOWN:
				switch (ev.key.keysym.sym)
				{
				case SDLK_DOWN:
					choise = (choise + 1) % 3;

					if (choise == 1)
					{
						button_options_object.src.x = 0;
						button_exit_object.src.x = 0;
						button_start_object.src.x = button_start_object.src.w;
					}

					if (choise == 0)
					{
						button_options_object.src.x = 0;
						button_start_object.src.x = 0;
						button_exit_object.src.x = button_exit_object.src.w;
					}

					if (choise == 2)
					{
						button_exit_object.src.x = 0;
						button_start_object.src.x = 0;
						button_options_object.src.x = button_options_object.src.w;
					}
					break;

				case SDLK_RETURN:
					if (choise == 2)
						is_options = true;
					else
						is_options = false;

					if (choise == 1)
						is_running = false;

					if (choise == 0)
					{
						SDL_DestroyTexture(button_start_texture);
						SDL_DestroyTexture(button_exit_texture);
						SDL_DestroyTexture(button_options_texture);

						SDL_DestroyTexture(text_1);
						SDL_DestroyTexture(text_2);
						SDL_DestroyTexture(text_3);
						SDL_DestroyTexture(text_4);
						is_running = false;
						return false;
					}
					break;

				case SDLK_ESCAPE:
					if (choise == 2)
						is_options = false;
					break;
				}
			}
		}
#pragma region drawing_menu

		SDL_SetRenderDrawColor(render, 255, 255, 255, 255);
		SDL_RenderClear(render);

		SDL_RenderCopy(render, background, &src, &dst); // отрисовка заднего фона меню

		if (is_options)
		{
			SDL_RenderCopy(render, text_1, NULL, &font_object.font_dst);
			SDL_RenderCopy(render, text_2, NULL, &font_object_1.font_dst);
			SDL_RenderCopy(render, text_3, NULL, &font_object_2.font_dst);
			SDL_RenderCopy(render, text_4, NULL, &font_object_3.font_dst);
		}
		else
		{
			/* отрисовка кнопок меню */
			SDL_RenderCopy(render, button_start_texture,
				&button_start_object.src, &button_start_object.dst);
			SDL_RenderCopy(render, button_exit_texture,
				&button_exit_object.src, &button_exit_object.dst);
			SDL_RenderCopy(render, button_options_texture,
				&button_options_object.src, &button_options_object.dst);
		}

#pragma endregion drawing_menu

		SDL_RenderPresent(render);
	}

	SDL_DestroyTexture(button_start_texture);
	SDL_DestroyTexture(button_exit_texture);
	SDL_DestroyTexture(button_options_texture);

	SDL_DestroyTexture(text_1);
	SDL_DestroyTexture(text_2);
	SDL_DestroyTexture(text_3);
	SDL_DestroyTexture(text_4);
	return true;
}

// функция для отображения смерти мышки от касания об стенку
void deadly_screen(SDL_Texture* background, SDL_Rect src, SDL_Rect dst)
{
	SDL_RenderClear(render);
	SDL_RenderCopy(render, background, &src, &dst);

	ObjectTexture dead_object;
	SDL_Texture* dead_texture = dead_object.create_texture("died.png");
	dead_object.set_dst(450, 300, 350, 300);

	SDL_RenderCopy(render, dead_texture, &dead_object.src, &dead_object.dst);
	SDL_RenderPresent(render);

	SDL_Delay(3000);

	SDL_DestroyTexture(dead_texture);
}

// функция для отображения победы 
void victory_screen(SDL_Texture* background, SDL_Rect src, SDL_Rect dst, SDL_Texture* font_texture, SDL_Rect font_dst)
{
	ObjectTexture victory_object;
	SDL_Texture* victory_texture = victory_object.create_texture("victory_sheet.png");
	victory_object.set_dst(450, 190, 300, 150);

	SDL_Event event;
	bool is_running = true;

	while (is_running)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					is_running = false;
				}
			}
		}

#pragma region drawing_victory
		SDL_RenderClear(render);
		SDL_RenderCopy(render, background, &src, &dst);

		SDL_RenderCopy(render, victory_texture, &victory_object.src, &victory_object.dst);
		SDL_RenderCopy(render, font_texture, NULL, &font_dst);

		SDL_RenderPresent(render);
#pragma endregion drawing_victory

	}

	SDL_DestroyTexture(victory_texture);
}

void Deinit_SDL2(SDL_Texture* background, SDL_Texture* mouse, SDL_Texture* key, SDL_Texture* font) // деинициализация всех компонентов подключенных библиотек
{
	std::cout << "Deinit start" << std::endl;
	SDL_DestroyTexture(key);
	SDL_DestroyTexture(mouse);
	SDL_DestroyTexture(background);
	SDL_DestroyTexture(font);

	SDL_DestroyRenderer(render);
	SDL_DestroyWindow(window);

	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
	std::cout << "Deinit end" << std::endl;
}

int main(int argc, char* argv[])
{
	Init_SDL2(SDL_INIT_VIDEO | SDL_INIT_AUDIO); // инициализируем видео и аудио
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048); // настраиваем звук
	Mix_Music* music = Mix_LoadMUS("swingin-and-singin.wav"); // загрузка трека в формате wav
	
	// массивы для хранения координат стенок лабиринта, а также их размеры
	int wall_x[MAX_COUNT]{};
	int wall_y[MAX_COUNT]{};
	int wall_w[MAX_COUNT]{};
	int wall_h[MAX_COUNT]{};

#pragma region bulean_variables
	bool mirror = false; // зеркальный рендер мышки на экране при беге влево
	bool is_mouse_button_click = false; // при нажатии л. кнопки мыши персонаж бежит за курсором
	bool is_start = false; // флаг нажатии кнопки start в меню
	bool is_running_game = true; // состояние игрового цикла
#pragma endregion bulean_variables

#pragma region loading_textures
	/* загрузка текстуры заднего фона */
	ObjectTexture background_object;
	SDL_Texture* background_texture = background_object.create_texture("background.jpg");
	background_object.set_dst(0, 0, window_width, window_height);

	/*загрузка текстуры мышки при беге вправо/влево */
	ObjectTexture mouse_left_right_object;
	SDL_Texture* mouse_left_right_texture = mouse_left_right_object.create_texture("mouse_running_left_right.png");
	mouse_left_right_object.set_src(0, 0, mouse_left_right_object.src.h + 3, mouse_left_right_object.src.h);
	mouse_left_right_object.set_dst(mouse_x, mouse_y, 75, 65);

	/* загрузка текстуры ключика в лабиринте */
	ObjectTexture key_object;
	SDL_Texture* key_texture = key_object.create_texture("key.png");
	key_object.set_dst(940, 265, 50, 50);

	/* загрузка текстуры смерти с косой */
	ObjectTexture fail_screen_object;
	SDL_Texture* fail_screen_texture = fail_screen_object.create_texture("died.png");
	fail_screen_object.set_dst(500, 380, 300, 260);
#pragma endregion loading_textures

	/* загрузка шрифта для времени и дальнейшего изменения */
	TTF_Font* font = TTF_OpenFont("ebrimabd.ttf", 32);
	char str[15] = "Time 00:00";
	SDL_Surface* surface_font = TTF_RenderText_Blended(font, str, { 255, 255, 255, 255 });
	SDL_Texture* texture_font = SDL_CreateTextureFromSurface(render, surface_font);
	SDL_Rect font_dst = {0, 0, surface_font->w, surface_font->h};
	
	/* переменные для подсчета времени */
	Uint32 start_time = 0;
	Uint32 elapsed_time = 0;

	int counter = 0;
	float len = 0;
	Mix_PlayMusic(music, -1);

		/* начало основного цикла игры */
		SDL_Event event;
		while (is_running_game) // основной игровой цикл
		{
			is_start = game_menu(background_texture, background_object.src, background_object.dst);
			if (!is_start)
				is_running_game = false;
			start_time = SDL_GetTicks();
			while (is_start) // цикл для запуска игры из меню
			{
				while (SDL_PollEvent(&event)) // цикл обработки событий с клавиатуры и комп. мышки
				{
					switch (event.type)
					{
					case SDL_QUIT:
						is_running_game = false;
						break;

					case SDL_WINDOWEVENT:
						if (event.window.event == SDL_WINDOWEVENT_RESIZED)
						{
							window_width = event.window.data1;
							window_height = event.window.data2;

							background_object.set_dst(0, 0, window_width, window_height);
						}
						break;

					case SDL_MOUSEMOTION:
						/* отслеживаем координаты комп. мыши и сохраняем */
						kursor_x = event.motion.x;
						kursor_y = event.motion.y;

						if (mouse_x > kursor_x)
							mirror = true;
						else
							mirror = false;
						break;

					case SDL_MOUSEBUTTONDOWN:
						if (event.button.button == SDL_BUTTON_LEFT)
							is_mouse_button_click = true;
						break;

					case SDL_MOUSEBUTTONUP:
						if (event.button.button == SDL_BUTTON_LEFT)
							is_mouse_button_click = false;
						break;
					}
				}

				if (surface_font != nullptr)
				{
					SDL_FreeSurface(surface_font);
				}

				if (texture_font != nullptr)
				{
					SDL_DestroyTexture(texture_font);
				}

				elapsed_time = (SDL_GetTicks() - start_time) / 1000;
				sprintf_s(str, "Time %02i:%02i", elapsed_time / 60, elapsed_time % 60);

				surface_font = TTF_RenderText_Blended(font, str, { 255, 255, 255, 255 });
				texture_font = SDL_CreateTextureFromSurface(render, surface_font);
				font_dst = { 0, 0, surface_font->w, surface_font->h };

#pragma region drawing
				SDL_RenderClear(render);

				/* отрисовка заднего фона */
				SDL_RenderCopy(render, background_texture, &background_object.src, &background_object.dst);

				SDL_RenderCopy(render, texture_font, NULL, &font_dst);

				counter = drawing_maze(wall_x, wall_y, wall_w, wall_h); // отрисовка стенок лабиринта

				/* обработка движений мышки (персонажа) при нажатии левой кнопки мыши */
				if (is_mouse_button_click)
				{
					/* движение мыши за курсором */
					len = move_mouse(mouse_x, mouse_y, kursor_x, kursor_y, FPS, speed);
					mouse_left_right_object.dst.x = mouse_x;
					mouse_left_right_object.dst.y = mouse_y;

					if (len > 30) // оставляем расстояние от курсора, чтобы не прилипала к нему
					{
						frame = (frame + 1) % count_frame;
						mouse_left_right_object.src.x = frame * mouse_left_right_object.src.w;
					}
					else
					{
						mouse_left_right_object.src.x = mouse_left_right_object.src.w * 2;
					}
				}
				/* проверка на столкновение со стенкой лабиринта */
				if (check_collision_wall(mouse_x, mouse_y, mouse_left_right_object.dst.w,
					mouse_left_right_object.dst.h, wall_x, wall_y, wall_w, wall_h, counter))
				{
					is_start = false;
					mouse_x = window_width / 100;
					mouse_y = window_height / 2;
					is_mouse_button_click = false;
					mouse_left_right_object.set_dst(mouse_x, mouse_y, 75, 65);
					deadly_screen(background_texture, background_object.src, background_object.dst);
				}

				/* проверка на столкновение с ключем (финиш) */
				if (check_collisoin_key(mouse_x, mouse_y, mouse_left_right_object.dst.w,
					mouse_left_right_object.dst.h, key_object.dst.x, key_object.dst.y))
				{
					is_start = false;
					mouse_x = window_width / 100;
					mouse_y = window_height / 2;
					mouse_left_right_object.set_dst(mouse_x, mouse_y, 75, 65);
					font_dst.x = 500;
					font_dst.y = 350;
					victory_screen(background_texture, background_object.src, background_object.dst, 
						texture_font, font_dst);
				}

				/* отрисовка мышки */
				if (!mirror)
					SDL_RenderCopy(render, mouse_left_right_texture,
						&mouse_left_right_object.src, &mouse_left_right_object.dst);
				else
					SDL_RenderCopyEx(render, mouse_left_right_texture,
						&mouse_left_right_object.src, &mouse_left_right_object.dst, 0, NULL, SDL_FLIP_HORIZONTAL);

				/* отрисовка ключа */
				SDL_RenderCopy(render, key_texture, &key_object.src, &key_object.dst);
#pragma endregion drawing

				SDL_RenderPresent(render); // обновление кадра после обработки событий и игровой логики 
				SDL_Delay(1000 / FPS); // задержка для статического FPS
			}
		}

	Mix_CloseAudio(); // закрытие потока фонового аудио
	Mix_FreeMusic(music); // освобождаем память для музыки
	SDL_FreeSurface(surface_font); // освобождение поверхности шрифта счетчика времени
	SDL_DestroyTexture(fail_screen_texture); // удаление текстуры смерти с косой
	Deinit_SDL2(background_texture, mouse_left_right_texture, key_texture, texture_font); // закрываем процессы
	return 0;
}