
class ScreenJob : Object UI
{
	int flags;
	float fadetime;	// in milliseconds
	int fadestate;

	int ticks;
	int jobstate;

	bool skipover;
	bool nowipe;

	enum EJobState
	{
		running = 0,	// normal operation
		skipped = 1,	// finished by user skipping
		finished = 2,	// finished by completing its sequence
		stopping = 3,	// running ending animations / fadeout, etc. Will not accept more input.
		stopped = 4,	// we're done here.
	};
	enum EJobFlags
	{
		visible = 0,
		fadein = 1,
		fadeout = 2,
		stopmusic = 4,
		stopsound = 8,
		transition_shift = 4,
		transition_mask = 48,
		transition_melt = 16,
		transition_burn = 32,
		transition_crossfade = 48,
	};

	void Init(int fflags = 0, float fadet = 250.f)
	{
		flags = fflags;
		fadetime = fadet;
		jobstate = running;
	}

	virtual bool ProcessInput()
	{
		return false;
	}

	virtual void Start() {}
	virtual bool OnEvent(InputEvent evt) { return false; }
	virtual void OnTick() {}
	virtual void Draw(double smoothratio) {}
	virtual void OnSkip() {}

	int DrawFrame(double smoothratio)
	{
		if (jobstate != running) smoothratio = 1; // this is necessary to avoid having a negative time span because the ticker won't be incremented anymore.
		Draw(smoothratio);
		if (jobstate == skipped) return -1;
		if (jobstate == finished) return 0;
		return 1;
	}

	int GetFadeState() { return fadestate; }
	override void OnDestroy()
	{
		if (flags & stopmusic) System.StopMusic();
		if (flags & stopsound) System.StopAllSounds();
	}

}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

class SkippableScreenJob : ScreenJob
{
	void Init(int flags = 0, float fadet = 250.f)
	{
		Super.Init(flags, fadet);
	}

	override bool OnEvent(InputEvent evt)
	{
		if (evt.type == InputEvent.Type_KeyDown && !System.specialKeyEvent(evt))
		{
			jobstate = skipped;
			OnSkip();
		}
		return true;
	}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

class BlackScreen : ScreenJob
{
	int wait;
	bool cleared;

	ScreenJob Init(int w, int flags = 0)
	{
		Super.Init(flags & ~(fadein|fadeout));
		wait = w;
		cleared = false;
		return self;
	}

	static ScreenJob Create(int w, int flags = 0)
	{
		return new("BlackScreen").Init(w, flags);
	}

	override void OnTick()
	{
		if (cleared)
		{
			int span = ticks * 1000 / GameTicRate;
			if (span > wait) jobstate = finished;
		}
	}

	override void Draw(double smooth)
	{
		cleared = true;
	}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

class ImageScreen : SkippableScreenJob
{
	int tilenum;
	int trans;
	int waittime; // in ms.
	bool cleared;
	TextureID texid;

	ScreenJob Init(TextureID tile, int fade = fadein | fadeout, int wait = 3000, int translation = 0)
	{
		Super.Init(fade);
		waittime = wait;
		texid = tile;
		trans = translation;
		cleared = false;
		return self;
	}

	ScreenJob InitNamed(String tex, int fade = fadein | fadeout, int wait = 3000, int translation = 0)
	{
		Super.Init(fade);
		waittime = wait;
		texid = TexMan.CheckForTexture(tex, TexMan.Type_Any, TexMan.TryAny | TexMan.ForceLookup);
		trans = translation;
		cleared = false;
		return self;
	}

	static ScreenJob Create(TextureID tile, int fade = fadein | fadeout, int wait = 3000, int translation = 0)
	{
		return new("ImageScreen").Init(tile, fade, wait, translation);
	}

	static ScreenJob CreateNamed(String tex, int fade = fadein | fadeout, int wait = 3000, int translation = 0)
	{
		return new("ImageScreen").InitNamed(tex, fade, wait, translation);
	}

	override void OnTick()
	{
		if (cleared)
		{
			int span = ticks * 1000 / GameTicRate;
			if (span > waittime) jobstate = finished;
		}
	}

	override void Draw(double smooth)
	{
		if (texid.IsValid()) Screen.DrawTexture(texid, true, 0, 0, DTA_FullscreenEx, FSMode_ScaleToFit43, DTA_LegacyRenderStyle, STYLE_Normal, DTA_TranslationIndex, trans);
		cleared = true;
	}
}

//---------------------------------------------------------------------------
//
// internal polymorphic movie player object
//
//---------------------------------------------------------------------------

struct MoviePlayer native
{
	enum EMovieFlags
	{
		NOSOUNDCUTOFF = 1,
		FIXEDVIEWPORT = 2,	// Forces fixed 640x480 screen size like for Blood's intros.
	}

	native static MoviePlayer Create(String filename, Array<int> soundinfo, int flags, int frametime, int firstframetime, int lastframetime);
	native void Start();
	native bool Frame(double clock);
	native void Destroy();
	native TextureID GetTexture();
}

//---------------------------------------------------------------------------
//
// 
//
//---------------------------------------------------------------------------

class MoviePlayerJob : SkippableScreenJob
{
	MoviePlayer player;
	bool started;
	int flag;

	ScreenJob Init(MoviePlayer mp, int flags)
	{
		Super.Init();
		flag = flags;
		player = mp;
		nowipe = true;	// due to synchronization issues wipes must be disabled on any movie.
		return self;
	}

	static ScreenJob CreateWithSoundInfo(String filename, Array<int> soundinfo, int flags, int frametime, int firstframetime = -1, int lastframetime = -1)
	{
		let movie = MoviePlayer.Create(filename, soundinfo, flags, frametime, firstframetime, lastframetime);
		if (movie) return new("MoviePlayerJob").Init(movie, flags);
		return null;
	}
	static ScreenJob Create(String filename, int flags, int frametime = -1)
	{
		Array<int> empty;
		return CreateWithSoundInfo(filename, empty, flags, frametime);
	}
	static ScreenJob CreateWithSound(String filename, Sound soundname, int flags, int frametime = -1)
	{
		Array<int> empty;
		empty.Push(1);
		empty.Push(int(soundname));
		return CreateWithSoundInfo(filename, empty, flags, frametime);
	}
	
	virtual void DrawFrame()
	{
		let tex = player.GetTexture();
		let size = TexMan.GetScaledSize(tex);
		
		if (!(flag & MoviePlayer.FIXEDVIEWPORT) || (size.x <= 320 && size.y <= 200) || size.x >= 640 || size.y >= 480)
		{
			Screen.DrawTexture(tex, false, 0, 0, DTA_FullscreenEx, FSMode_ScaleToFit43, DTA_Masked, false);
		}
		else
		{
			Screen.DrawTexture(tex, false, 320, 240, DTA_VirtualWidth, 640, DTA_VirtualHeight, 480, DTA_CenterOffset, true, DTA_Masked, false);
		}

	}

	override void Draw(double smoothratio)
	{
		if (!player)
		{
			jobstate = stopped;
			return;
		}
		if (!started)
		{
			started = true;
			player.Start();
		}
		double clock = (ticks + smoothratio) * 1000000000. / GameTicRate;
		if (jobstate == running  && !player.Frame(clock))
		{
			jobstate = finished;
		}
		DrawFrame();
	}

	override void OnDestroy()
	{
		if (player)
		{
			player.Destroy();
		}
		player = null;
	}
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

class ScreenJobRunner : Object UI
{
	enum ERunState
	{
		State_Clear,
		State_Run,
		State_Fadeout,
	}
	Array<ScreenJob> jobs;
	//CompletionFunc completion;
	int index;
	float screenfade;
	bool clearbefore;
	bool skipall;
	bool advance;
	int actionState;
	int terminateState;
	int fadeticks;
	int last_paused_tic;
	
	native static void setTransition(int type);

	void Init(bool clearbefore_, bool skipall_)
	{
		clearbefore = clearbefore_;
		skipall = skipall_;
		index = -1;
		fadeticks = 0;
		last_paused_tic = -1;
	}

	override void OnDestroy()
	{
		DeleteJobs();
	}

	protected void DeleteJobs()
	{
		// Free all allocated resources now.
		for (int i = 0; i < jobs.Size(); i++)
		{
			if (jobs[i]) jobs[i].Destroy();
		}
		jobs.Clear();
	}

	void Append(ScreenJob job)
	{
		if (job != null) jobs.Push(job);
	}

	virtual bool Validate()
	{
		return jobs.Size() > 0;
	}

	bool CanWipe()
	{
		if (index < jobs.Size()) return !jobs[index].nowipe;
		return true;		
	}

	//---------------------------------------------------------------------------
	//
	// 
	//
	//---------------------------------------------------------------------------

	protected void AdvanceJob(bool skip)
	{
		if (index == jobs.Size()-1) 
		{
			index++;
			return; // we need to retain the last element until the runner is done.
		}

		if (index >= 0) jobs[index].Destroy();
		index++;
		while (index < jobs.Size() && (jobs[index] == null || (skip && jobs[index].skipover)))
		{
			if (jobs[index] != null && index < jobs.Size() - 1) jobs[index].Destroy(); // may not delete the last element - we still need it for shutting down.
			index++;
		}
		actionState = clearbefore ? State_Clear : State_Run;
		if (index < jobs.Size())
		{
			jobs[index].fadestate = !paused && jobs[index].flags & ScreenJob.fadein? ScreenJob.fadein : ScreenJob.visible;
			jobs[index].Start();
			if (jobs[index].flags & ScreenJob.transition_mask)
			{
				setTransition((jobs[index].flags & ScreenJob.transition_mask) >> ScreenJob.Transition_Shift);
			}
		}
	}

	//---------------------------------------------------------------------------
	//
	// 
	//
	//---------------------------------------------------------------------------

	virtual int DisplayFrame(double smoothratio)
	{
		if (jobs.Size() == 0) 
		{
			return 1;
		}
		int x = index >= jobs.Size()? jobs.Size()-1 : index;
		let job = jobs[x];
		bool processed = job.ProcessInput();

		if (job.fadestate == ScreenJob.fadein)
		{
			double ms = (job.ticks + smoothratio) * 1000 / GameTicRate / job.fadetime;
			double screenfade = clamp(ms, 0., 1.);
			Screen.SetScreenFade(screenfade);
			if (screenfade == 1.) job.fadestate = ScreenJob.visible;
		}
		int state = job.DrawFrame(smoothratio);
		Screen.SetScreenFade(1.);
		return state;
	}

	//---------------------------------------------------------------------------
	//
	// 
	//
	//---------------------------------------------------------------------------

	virtual int FadeoutFrame(double smoothratio)
	{
		int x = index >= jobs.Size()? jobs.Size()-1 : index;
		let job = jobs[x];
		double ms = (fadeticks + smoothratio) * 1000 / GameTicRate / job.fadetime;
		float screenfade = 1. - clamp(ms, 0., 1.);
		Screen.SetScreenFade(screenfade);
		job.DrawFrame(1.);
		Screen.SetScreenFade(1.);
		return (screenfade > 0.);
	}

	//---------------------------------------------------------------------------
	//
	// 
	//
	//---------------------------------------------------------------------------

	virtual bool OnEvent(InputEvent ev)
	{
		if (paused || index < 0 || index >= jobs.Size()) return false;
		if (jobs[index].jobstate != ScreenJob.running) return false;
		return jobs[index].OnEvent(ev);
	}

	//---------------------------------------------------------------------------
	//
	// 
	//
	//---------------------------------------------------------------------------

	virtual bool OnTick()
	{
		if (paused) return false;
		if (index >= jobs.Size() || jobs.Size() == 0) return true;
		if (advance || index < 0)
		{
			advance = false;
			AdvanceJob(terminateState < 0);
			if (index >= jobs.Size())
			{
				return true;
			}
		}
		if (jobs[index].jobstate == ScreenJob.running)
		{
			jobs[index].ticks++;
			jobs[index].OnTick();
		}
		else if (jobs[index].jobstate == ScreenJob.stopping)
		{
			fadeticks++;
		}
		return false;
	}

	//---------------------------------------------------------------------------
	//
	// 
	//
	//---------------------------------------------------------------------------

	virtual bool RunFrame(double smoothratio)
	{
		if (index < 0)
		{
			AdvanceJob(false);
		}
		// ensure that we won't go back in time if the menu is dismissed without advancing our ticker
		if (index < jobs.Size())
		{
			bool menuon = paused;
			if (menuon) last_paused_tic = jobs[index].ticks;
			else if (last_paused_tic == jobs[index].ticks) menuon = true;
			if (menuon) smoothratio = 1.;
		}
		else smoothratio = 1.;

		if (actionState == State_Clear)
		{
			actionState = State_Run;
		}
		else if (actionState == State_Run)
		{
			terminateState = DisplayFrame(smoothratio);
			if (terminateState < 1 && index < jobs.Size())
			{
				if (jobs[index].flags & ScreenJob.fadeout)
				{
					jobs[index].fadestate = ScreenJob.fadeout;
					jobs[index].jobstate = ScreenJob.stopping;
					actionState = State_Fadeout;
					fadeticks = 0;
				}
				else
				{
					advance = true;
				}
			}
		}
		else if (actionState == State_Fadeout)
		{
			int ended = FadeoutFrame(smoothratio);
			if (ended < 1 && index < jobs.Size())
			{
				jobs[index].jobstate = ScreenJob.stopped;
				advance = true;
			}
		}
		return true;
	}

	void AddGenericVideo(String fn, int snd, int framerate)
	{
		Array<int> sounds;
		if (snd > 0) sounds.Pushv(1, snd);
		Append(MoviePlayerJob.CreateWithSoundInfo(fn, sounds, 0, framerate));
	}

	//==========================================================================
	//
	// This also gets used by the title loop.
	//
	//==========================================================================

	void DrawFullscreenSubtitle(String text, int cr)
	{
		if (text.length() == 0 || !inter_subtitles) return;

		// This uses the same scaling as regular HUD messages
		let screensize = Screen.GetTextScreenSize();
		let hudwidth = screensize.X;
		let hudheight = screensize.Y;
		let hscale = screen.GetWidth() / hudwidth;
		let vscale = screen.GetHeight() / hudheight;
		let font = generic_ui? NewSmallFont : SmallFont;

		let linelen = hudwidth < 640 ? hudwidth * 0.9 - 40 : 560;
		let lines = font.BreakLines(text, int(linelen));

		int count = lines.Count();
		int height = 20 + font.GetHeight() * count;

		double x, y, w;

		if (linelen < 560)
		{
			x = hudwidth / 20;
			w = hudwidth - 2 * x;
		}
		else
		{
			x = (hudwidth * 0.5) - 300;
			w = 600;
		}
		y = hudheight * 0.9 - height;
		if (y < 0) y = 0;

		Screen.Dim(0, 0.5f, int(x * hscale), int(y * vscale), int(w * hscale), int(height * vscale));
		x += 20;
		y += 10;

		for (int i = 0; i < count; i++)
		{
			Screen.DrawText(font, cr, x, y, lines.StringAt(i), DTA_KeepRatio, true, DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);
			y += font.GetHeight();
		}
	}
}

//---------------------------------------------------------------------------
//
// scrolling text, like Exhumed's story screens.
//
//---------------------------------------------------------------------------

class TextOverlay
{
	int nHeight;
	double nCrawlY;
	int palette;
	int crange;
	bool drawclean;
	BrokenLines screentext;
	Font myfont;

	void Init(String text, int cr = Font.CR_NATIVEPAL, int pal = 0, bool clean = false)
	{
		myfont = SmallFont; // todo
		screentext = myfont.BreakLines(StringTable.Localize(text), 320);
		nCrawlY = 199;
		nHeight = screentext.Count() * 10;
		palette = pal;
		crange = cr;
		drawclean = clean;
	}

	void DisplayText()
	{
		if (nHeight + nCrawlY > 0)
		{
			double y = nCrawlY;
			for (int i = 0; i < screentext.Count() && y <= 199; i++)
			{
				if (y >= -10) 
				{
					int x = 160 - screenText.StringWidth(i)/2;
					if (!drawclean) Screen.DrawText(myfont, crange, x, y, screentext.StringAt(i), DTA_FullscreenScale, FSMode_Fit320x200, DTA_TranslationIndex, palette);
					else Screen.DrawText(myfont, crange, x, y, screentext.StringAt(i), DTA_Clean, true, DTA_TranslationIndex, palette);
				}
				y += 10;
			}
		}
	}

	bool ScrollText(double clock)
	{
		if (nHeight + nCrawlY > 0 || musplaying.handle)
		{
			nCrawlY = 199 - clock / 15.;
			return false;
		}
		return true;
	}
}

//==========================================================================
//
// type-on text, like Doom
//
//==========================================================================

class TextTypeOnOverlay
{
	String mText;
	Font mFont;
	int mTextSpeed;
	int mTextX, mTextY;
	int mTextCounter;
	int mTextLen;
	int mTextColor;
	int mPalette;
	int mLineCount;
	int mRowPadding;
	bool usesDefault;
	int mTicker;
	int mTextDelay;

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void Init(Font fnt, String text, int x = 10, int y = 10, int rowpadding = 2, int speed = 2, int cr = Font.CR_NATIVEPAL, int pal = 0)
	{
		let tt = StringTable.Localize(text);
		Array<String> lines;

		tt.Split(lines, "\n");
		mLineCount = lines.Size();

		mText = "";
		for (int i = 0; i < lines.Size(); i++)
		{
			tt = lines[i];
			tt.StripRight();
			mText.AppendFormat("\n%s", tt);
		}

		mTextSpeed = speed;
		if (x < 0)
		{
			mTextX = mTextY = 10;
			usesDefault = true;
		}
		else
		{
			mTextX = x;
			mTextY = y;
		}

		if (!generic_ui)
		{
			// Todo: Split too long texts

			// If the text is too wide, center it so that it works better on widescreen displays.
			// On 4:3 it'd still be cut off, though.
			int width = fnt.StringWidth(mText);
			if (usesDefault && mTextX + width > 320 - mTextX)
			{
				mTextX = (320 - width) / 2;
			}
		}
		else
		{
			// Todo: Split too long texts

			mTextX *= 2;
			mTextY *= 2;
			int width = NewSmallFont.StringWidth(mText);
			if (usesDefault && mTextX + width > 640 - mTextX)
			{
				mTextX = (640 - width) / 2;
			}
		}


		mTextLen = mText.CodePointCount();
		mTextColor = cr;
		mPalette = pal;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	bool Tick()
	{
		if (mTicker < mTextDelay + (mTextLen * mTextSpeed))
		{
			mTicker++;
			return false;
		}
		return true;
	}

	bool Reveal()
	{
		if (mTicker < mTextDelay + (mTextLen * mTextSpeed))
		{
			mTicker = mTextDelay + (mTextLen * mTextSpeed);
			return true;
		}
		return false;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void Drawer ()
	{
		if (mTicker >= mTextDelay)
		{
			int w;
			int count;
			int c;
			let width = Screen.GetWidth();
			let height = Screen.GetHeight();

			// Count number of rows in this text. Since it does not word-wrap, we just count
			// line feed characters.
			let font = generic_ui ? NewSmallFont : SmallFont;
			let fontscale = MAX(generic_ui ? MIN(width / 640, height / 400) : MIN(width / 400, height / 250), 1);
			int cleanwidth = width / fontscale;
			int cleanheight = height / fontscale;
			int refwidth = generic_ui ? 640 : 320;
			int refheight = generic_ui ? 400 : 200;
			int kerning = font.GetDefaultKerning();


			int rowheight = font.GetHeight() * fontscale;
			int rowpadding = mRowPadding;

			int cx = (mTextX - refwidth/2) * fontscale + width / 2;
			int cy = (mTextY - refheight/2) * fontscale + height / 2;
			cx = MAX(0, cx);
			int startx = cx;

			if (usesDefault)
			{
				int allheight;
				while ((allheight = mLineCount * (rowheight + rowpadding)) > height && rowpadding > 0)
				{
					rowpadding--;
				}
				allheight = mLineCount * (rowheight + rowpadding);
				if (height - cy - allheight < cy)
				{
					cy = (height - allheight) / 2;
					if (cy < 0) cy = 0;
				}
			}
			else
			{
				// Does this text fall off the end of the screen? If so, try to eliminate some margins first.
				while (rowpadding > 0 && cy + mLineCount * (rowheight + rowpadding) - rowpadding > height)
				{
					rowpadding--;
				}
				// If it's still off the bottom, you are screwed if the origin is fixed.
			}
			rowheight += rowpadding;

			// draw some of the text onto the screen
			int curpos = 0;
			for (count = (mTicker - mTextDelay) / mTextSpeed; count > 0 ; count-- )
			{
				[c, curpos] = mText.GetNextCodePoint(curpos);
				if (!c)
					break;
				if (c == "\n")
				{
					cx = startx;
					cy += rowheight;
					continue;
				}

				w = font.GetCharWidth(c);
				w += kerning;
				w *= fontscale;
				if (cx + w > width)
					continue;

				Screen.DrawChar(font, mTextColor, cx/fontscale, cy/fontscale, c, DTA_KeepRatio, true, DTA_VirtualWidth, cleanwidth, DTA_VirtualHeight, cleanheight);
				cx += w;
			}
		}
	}
}