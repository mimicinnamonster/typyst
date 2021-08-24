int
sdlexit()
{
	//Destroy window
	SDL_DestroyWindow(sdlw.wnd);

	//Quit SDL subsystems
	SDL_Quit();
}

int
sdlinit()
{
	TTF_Font *ttffont = 0;

	// load font
	{
		unsigned char *fontfile = 0;
		
		if (!FcInit()) die("could not init fontconfig.\n");
		usedfont = (opt_font == NULL)? font : opt_font; 

		// font config matching bullshit
		{
			FcPattern *pattern = FcNameParse((const FcChar8 *)usedfont);
			FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
			FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_MEDIUM);
			FcPattern *configured = FcPatternDuplicate(pattern);
			FcConfigSubstitute(NULL, configured, FcMatchPattern);
			FcResult result;
			FcPattern *match = FcFontMatch(NULL, configured, &result);
			FcPatternGetString(match, FC_FILE, 0, &fontfile);

			FcPatternGetDouble(match, FC_PIXEL_SIZE, 0, &usedfontsize);
			defaultfontsize = usedfontsize;

      printf("file: %s %f\n", fontfile, usedfontsize);
		}

		if (TTF_Init() == -1) die("TTF_Init failed");

		// load font
    ttffont = TTF_OpenFont(fontfile, 20);
		if (!ttffont) die("TTF_OpenFont couldn't load the font");
    TTF_SetFontHinting(ttffont, TTF_HINTING_LIGHT);
	}
	
  // screen size based on glyph width
	{
		int minx=0, maxx=0;

    TTF_GlyphMetrics(ttffont, ' ', &minx, &maxx, 0, 0, 0);
		win.cw = maxx - minx;
		win.cw = 10;
		win.ch = TTF_FontHeight(ttffont);
		win.w = 2 * borderpx + cols * win.cw;
		win.h = 2 * borderpx + rows * win.ch;
		printf("width %d height %d cols %d rows %d\n", win.cw, win.ch, cols, rows);
	}

	// prepare sdl window
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0) die("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		
		//Create window
		sdlw.wnd = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, win.w, win.h, SDL_WINDOW_SHOWN);
		if(sdlw.wnd == NULL) die("Window could not be created! SDL_Error: %s\n", SDL_GetError());

		//Get window surface
		sdlw.srf = SDL_GetWindowSurface(sdlw.wnd);

		//Fill the surface white
		SDL_FillRect(sdlw.srf, NULL, SDL_MapRGB(sdlw.srf->format, 0x0, 0x0, 0x0));
		
	}
	
	// print debug text
	{
		SDL_Surface* surface = TTF_RenderUTF8_Blended(ttffont, "😄", (SDL_Color){255, 255, 255});
		SDL_BlitSurface(surface, NULL, sdlw.srf, NULL);
	}
	
	//Update the surface
	SDL_UpdateWindowSurface(sdlw.wnd);

	//Wait two seconds
	SDL_Delay(2000);
}
