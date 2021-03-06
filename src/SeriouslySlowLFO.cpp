#include "FrozenWasteland.hpp"
#include "dsp/digital.hpp"

struct LowFrequencyOscillator {
	float phase = 0.0;
	float pw = 0.5;
	float freq = 1.0;
	bool offset = false;
	bool invert = false;
	SchmittTrigger resetTrigger;
	LowFrequencyOscillator() {
		resetTrigger.setThresholds(0.0, 0.01);
	}
	void setPitch(float pitch) {
		pitch = fminf(pitch, 8.0);
		freq = powf(2.0, pitch);
	}
	void setFrequency(float frequency) {
		freq = frequency;
	}
	void setPulseWidth(float pw_) {
		const float pwMin = 0.01;
		pw = clampf(pw_, pwMin, 1.0 - pwMin);
	}
	void setReset(float reset) {
		if (resetTrigger.process(reset)) {
			phase = 0.0;
		}
	}
	void hardReset()
	{
		phase = 0.0;
	}

	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5);
		phase += deltaPhase;
		if (phase >= 1.0)
			phase -= 1.0;
	}
	float sin() {
		if (offset)
			return 1.0 - cosf(2*M_PI * phase) * (invert ? -1.0 : 1.0);
		else
			return sinf(2*M_PI * phase) * (invert ? -1.0 : 1.0);
	}
	float tri(float x) {
		return 4.0 * fabsf(x - roundf(x));
	}
	float tri() {
		if (offset)
			return tri(invert ? phase - 0.5 : phase);
		else
			return -1.0 + tri(invert ? phase - 0.25 : phase - 0.75);
	}
	float saw(float x) {
		return 2.0 * (x - roundf(x));
	}
	float saw() {
		if (offset)
			return invert ? 2.0 * (1.0 - phase) : 2.0 * phase;
		else
			return saw(phase) * (invert ? -1.0 : 1.0);
	}
	float sqr() {
		float sqr = (phase < pw) ^ invert ? 1.0 : -1.0;
		return offset ? sqr + 1.0 : sqr;
	}
	float progress() {
		return phase;
	}
};



struct SeriouslySlowLFO : Module {
	enum ParamIds {
		TIME_BASE_PARAM,
		DURATION_PARAM,		
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SIN_OUTPUT,
		TRI_OUTPUT,
		SAW_OUTPUT,
		SQR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		MINUTES_LIGHT,
		HOURS_LIGHT,
		DAYS_LIGHT,
		WEEKS_LIGHT,
		MONTHS_LIGHT,
		NUM_LIGHTS
	};	
	


	LowFrequencyOscillator oscillator;
	SchmittTrigger sumTrigger;
	float duration = 0.0;
	int timeBase = 0;


	SeriouslySlowLFO() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	json_t *toJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "timeBase", json_integer((int) timeBase));
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "timeBase");
		if (sumJ)
			timeBase = json_integer_value(sumJ);
	}

	void reset() override {
		timeBase = 0;
	}

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void SeriouslySlowLFO::step() {

	if (sumTrigger.process(params[TIME_BASE_PARAM].value)) {
		timeBase = (timeBase + 1) % 5;
		oscillator.hardReset();
	}

	float numberOfSeconds = 0;
	switch(timeBase) {
		case 0 :
			numberOfSeconds = 60; // Minutes
			break;
		case 1 :
			numberOfSeconds = 3600; // Hours
			break;
		case 2 :
			numberOfSeconds = 86400; // Days
			break;
		case 3 :
			numberOfSeconds = 604800; // Weeks
			break;
		case 4 :
			numberOfSeconds = 259200; // Months
			break;
	}

	duration = params[DURATION_PARAM].value;
	if(inputs[FM_INPUT].active) {
		duration +=inputs[FM_INPUT].value;
	}
	duration = clampf(duration,1.0,100.0);
	
	oscillator.setFrequency(1.0 / (duration * numberOfSeconds));
	oscillator.step(1.0 / engineGetSampleRate());
	if(inputs[RESET_INPUT].active) {
		oscillator.setReset(inputs[RESET_INPUT].value);
	} 


	outputs[SIN_OUTPUT].value = 5.0 * oscillator.sin();
	outputs[TRI_OUTPUT].value = 5.0 * oscillator.tri();
	outputs[SAW_OUTPUT].value = 5.0 * oscillator.saw();
	outputs[SQR_OUTPUT].value =  5.0 * oscillator.sqr();
	
	for(int lightIndex = 0;lightIndex<5;lightIndex++)
	{
		lights[lightIndex].value = lightIndex != timeBase ? 0.0 : 1.0;
	}
}

struct LFOProgressDisplay : TransparentWidget {
	SeriouslySlowLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	LFOProgressDisplay() {
		font = Font::load(assetPlugin(plugin, "res/fonts/01 Digit.ttf"));
	}

	void drawProgress(NVGcontext *vg, float phase) 
	{
		//float startArc = (-M_PI) / 2.0; we know this rotates 90 degrees to top
		//float endArc = (phase * M_PI) - startArc;
		const float rotate90 = (M_PI) / 2.0;
		float startArc = 0 - rotate90;
		float endArc = (phase * M_PI * 2) - rotate90;

		// Draw indicator
		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		{
			nvgBeginPath(vg);
			nvgArc(vg,109.8,184.5,35,startArc,endArc,NVG_CW);
			nvgLineTo(vg,109.8,184.5);
			nvgClosePath(vg);
		}
		nvgFill(vg);
	}

	void drawDuration(NVGcontext *vg, Vec pos, float duration) {
		nvgFontSize(vg, 28);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % #6.1f", duration);
		nvgText(vg, pos.x + 22, pos.y, text, NULL);
	}

	void draw(NVGcontext *vg) override {
	
		drawProgress(vg,module->oscillator.progress());
		drawDuration(vg, Vec(0, box.size.y - 150), module->duration);
	}
};

SeriouslySlowLFOWidget::SeriouslySlowLFOWidget() {
	SeriouslySlowLFO *module = new SeriouslySlowLFO();
	setModule(module);
	box.size = Vec(15*10, RACK_GRID_HEIGHT);
	
	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/SeriouslySlowLFO.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	{
		LFOProgressDisplay *display = new LFOProgressDisplay();
		display->module = module;
		display->box.pos = Vec(0, 0);
		display->box.size = Vec(box.size.x, 220);
		addChild(display);
	}

	addParam(createParam<CKD6>(Vec(10, 110), module, SeriouslySlowLFO::TIME_BASE_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(65, 85), module, SeriouslySlowLFO::DURATION_PARAM, 1.0, 100.0, 1.0));

	addInput(createInput<PJ301MPort>(Vec(11, 270), module, SeriouslySlowLFO::FM_INPUT));
	addInput(createInput<PJ301MPort>(Vec(91, 270), module, SeriouslySlowLFO::RESET_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, SeriouslySlowLFO::SIN_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, SeriouslySlowLFO::TRI_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, SeriouslySlowLFO::SAW_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, SeriouslySlowLFO::SQR_OUTPUT));

	addChild(createLight<MediumLight<BlueLight>>(Vec(10, 158), module, SeriouslySlowLFO::MINUTES_LIGHT));
	addChild(createLight<MediumLight<BlueLight>>(Vec(10, 173), module, SeriouslySlowLFO::HOURS_LIGHT));
	addChild(createLight<MediumLight<BlueLight>>(Vec(10, 188), module, SeriouslySlowLFO::DAYS_LIGHT));
	addChild(createLight<MediumLight<BlueLight>>(Vec(10, 203), module, SeriouslySlowLFO::WEEKS_LIGHT));
	addChild(createLight<MediumLight<BlueLight>>(Vec(10, 218), module, SeriouslySlowLFO::MONTHS_LIGHT));
}
