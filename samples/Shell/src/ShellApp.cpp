#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "v8.h"

v8::Handle<v8::Context> CreateShellContext(v8::Isolate* isolate);
void RunShell(v8::Handle<v8::Context> context);
int RunMain(v8::Isolate* isolate, int argc, char* argv[]);
bool ExecuteString(v8::Isolate* isolate,
                   v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions);
void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
void Load(const v8::FunctionCallbackInfo<v8::Value>& args);
void Quit(const v8::FunctionCallbackInfo<v8::Value>& args);
void Version(const v8::FunctionCallbackInfo<v8::Value>& args);
v8::Handle<v8::String> ReadFile(v8::Isolate* isolate, const char* name);
void ReportException(v8::Isolate* isolate, v8::TryCatch* handler);


static bool run_shell;

using namespace ci;
using namespace ci::app;
using namespace std;

class ProcessApp : public AppNative {
public:
	void setup();
	void mouseDown( MouseEvent event );
	void update();
	void draw();
};

void ProcessApp::setup()
{
	// This sample takes filenames as arguments then reads and executes their contents. Includes a command prompt at which you can enter JavaScript code snippets which are then executed. In this sample additional functions like print are also added to JavaScript through the use of object and function templates.
	int argc = 0;
	char *argv = nullptr;
	v8::V8::InitializeICU();
	v8::V8::SetFlagsFromCommandLine( &argc, &argv, true);
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	run_shell = (1 == 1);
	int result;
	{
		v8::HandleScope handle_scope(isolate);
		v8::Handle<v8::Context> context = CreateShellContext(isolate);
		if (context.IsEmpty()) {
			fprintf(stderr, "Error creating context\n");
			quit();
		}
		context->Enter();
		result = RunMain( isolate, argc, &argv );
		if (run_shell) RunShell(context);
		context->Exit();
	}
	v8::V8::Dispose();
	
}

void ProcessApp::mouseDown( MouseEvent event )
{
}

void ProcessApp::update()
{
}

void ProcessApp::draw()
{
	// clear out the window with black
	gl::clear( Color( 0, 0, 0 ) );
}

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
	return *value ? *value : "<string conversion failed>";
}


// Creates a new execution environment containing the built-in
// functions.
v8::Handle<v8::Context> CreateShellContext(v8::Isolate* isolate) {
	// Create a template for the global object.
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
	// Bind the global 'print' function to the C++ Print callback.
	global->Set(v8::String::NewFromUtf8(isolate, "print"),
				v8::FunctionTemplate::New(isolate, Print));
	// Bind the global 'read' function to the C++ Read callback.
	global->Set(v8::String::NewFromUtf8(isolate, "read"),
				v8::FunctionTemplate::New(isolate, Read));
	// Bind the global 'load' function to the C++ Load callback.
	global->Set(v8::String::NewFromUtf8(isolate, "load"),
				v8::FunctionTemplate::New(isolate, Load));
	// Bind the 'quit' function
	global->Set(v8::String::NewFromUtf8(isolate, "quit"),
				v8::FunctionTemplate::New(isolate, Quit));
	// Bind the 'version' function
	global->Set(v8::String::NewFromUtf8(isolate, "version"),
				v8::FunctionTemplate::New(isolate, Version));
	
	return v8::Context::New(isolate, NULL, global);
}


// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
	bool first = true;
	for (int i = 0; i < args.Length(); i++) {
		v8::HandleScope handle_scope(args.GetIsolate());
		if (first) {
			first = false;
		} else {
			printf(" ");
		}
		v8::String::Utf8Value str(args[i]);
		const char* cstr = ToCString(str);
		printf("%s", cstr);
	}
	printf("\n");
	fflush(stdout);
}


// The callback that is invoked by v8 whenever the JavaScript 'read'
// function is called.  This function loads the content of the file named in
// the argument into a JavaScript string.
void Read(const v8::FunctionCallbackInfo<v8::Value>& args) {
	if (args.Length() != 1) {
		args.GetIsolate()->ThrowException(
										  v8::String::NewFromUtf8(args.GetIsolate(), "Bad parameters"));
		return;
	}
	v8::String::Utf8Value file(args[0]);
	if (*file == NULL) {
		args.GetIsolate()->ThrowException(
										  v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
		return;
	}
	v8::Handle<v8::String> source = ReadFile(args.GetIsolate(), *file);
	if (source.IsEmpty()) {
		args.GetIsolate()->ThrowException(
										  v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
		return;
	}
	args.GetReturnValue().Set(source);
}


// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
void Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
	for (int i = 0; i < args.Length(); i++) {
		v8::HandleScope handle_scope(args.GetIsolate());
		v8::String::Utf8Value file(args[i]);
		if (*file == NULL) {
			args.GetIsolate()->ThrowException(
											  v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
			return;
		}
		v8::Handle<v8::String> source = ReadFile(args.GetIsolate(), *file);
		if (source.IsEmpty()) {
			args.GetIsolate()->ThrowException(
											  v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
			return;
		}
		if (!ExecuteString(args.GetIsolate(),
						   source,
						   v8::String::NewFromUtf8(args.GetIsolate(), *file),
						   false,
						   false)) {
			args.GetIsolate()->ThrowException(
											  v8::String::NewFromUtf8(args.GetIsolate(), "Error executing file"));
			return;
		}
	}
}


// The callback that is invoked by v8 whenever the JavaScript 'quit'
// function is called.  Quits.
void Quit(const v8::FunctionCallbackInfo<v8::Value>& args) {
	// If not arguments are given args[0] will yield undefined which
	// converts to the integer value 0.
	int exit_code = args[0]->Int32Value();
	fflush(stdout);
	fflush(stderr);
	exit(exit_code);
}


void Version(const v8::FunctionCallbackInfo<v8::Value>& args) {
	args.GetReturnValue().Set(
							  v8::String::NewFromUtf8(args.GetIsolate(), v8::V8::GetVersion()));
}


// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(v8::Isolate* isolate, const char* name) {
	FILE* file = fopen(name, "rb");
	if (file == NULL) return v8::Handle<v8::String>();
	
	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	rewind(file);
	
	char* chars = new char[size + 1];
	chars[size] = '\0';
	for (int i = 0; i < size;) {
		int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
		i += read;
	}
	fclose(file);
	v8::Handle<v8::String> result =
	v8::String::NewFromUtf8(isolate, chars, v8::String::kNormalString, size);
	delete[] chars;
	return result;
}


// Process remaining command line arguments and execute files
int RunMain(v8::Isolate* isolate, int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		const char* str = argv[i];
		if (strcmp(str, "--shell") == 0) {
			run_shell = true;
		} else if (strcmp(str, "-f") == 0) {
			// Ignore any -f flags for compatibility with the other stand-
			// alone JavaScript engines.
			continue;
		} else if (strncmp(str, "--", 2) == 0) {
			fprintf(stderr,
					"Warning: unknown flag %s.\nTry --help for options\n", str);
		} else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
			// Execute argument given to -e option directly.
			v8::Handle<v8::String> file_name =
			v8::String::NewFromUtf8(isolate, "unnamed");
			v8::Handle<v8::String> source =
			v8::String::NewFromUtf8(isolate, argv[++i]);
			if (!ExecuteString(isolate, source, file_name, false, true)) return 1;
		} else {
			// Use all other arguments as names of files to load and run.
			v8::Handle<v8::String> file_name = v8::String::NewFromUtf8(isolate, str);
			v8::Handle<v8::String> source = ReadFile(isolate, str);
			if (source.IsEmpty()) {
				fprintf(stderr, "Error reading '%s'\n", str);
				continue;
			}
			if (!ExecuteString(isolate, source, file_name, false, true)) return 1;
		}
	}
	return 0;
}


// The read-eval-execute loop of the shell.
void RunShell(v8::Handle<v8::Context> context) {
	fprintf(stderr, "V8 version %s [sample shell]\n", v8::V8::GetVersion());
	static const int kBufferSize = 256;
	// Enter the execution environment before evaluating any code.
	v8::Context::Scope context_scope(context);
	v8::Local<v8::String> name(
							   v8::String::NewFromUtf8(context->GetIsolate(), "(shell)"));
	while (true) {
		char buffer[kBufferSize];
		fprintf(stderr, "> ");
		char* str = fgets(buffer, kBufferSize, stdin);
		if (str == NULL) break;
		v8::HandleScope handle_scope(context->GetIsolate());
		ExecuteString(context->GetIsolate(),
					  v8::String::NewFromUtf8(context->GetIsolate(), str),
					  name,
					  true,
					  true);
	}
	fprintf(stderr, "\n");
}


// Executes a string within the current v8 context.
bool ExecuteString(v8::Isolate* isolate,
                   v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions) {
	v8::HandleScope handle_scope(isolate);
	v8::TryCatch try_catch;
	v8::ScriptOrigin origin(name);
	v8::Handle<v8::Script> script = v8::Script::Compile(source, &origin);
	if (script.IsEmpty()) {
		// Print errors that happened during compilation.
		if (report_exceptions)
			ReportException(isolate, &try_catch);
		return false;
	} else {
		v8::Handle<v8::Value> result = script->Run();
		if (result.IsEmpty()) {
			assert(try_catch.HasCaught());
			// Print errors that happened during execution.
			if (report_exceptions)
				ReportException(isolate, &try_catch);
			return false;
		} else {
			assert(!try_catch.HasCaught());
			if (print_result && !result->IsUndefined()) {
				// If all went well and the result wasn't undefined then print
				// the returned value.
				v8::String::Utf8Value str(result);
				const char* cstr = ToCString(str);
				printf("%s\n", cstr);
			}
			return true;
		}
	}
}


void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
	v8::HandleScope handle_scope(isolate);
	v8::String::Utf8Value exception(try_catch->Exception());
	const char* exception_string = ToCString(exception);
	v8::Handle<v8::Message> message = try_catch->Message();
	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just
		// print the exception.
		fprintf(stderr, "%s\n", exception_string);
	} else {
		// Print (filename):(line number): (message).
		v8::String::Utf8Value filename(message->GetScriptResourceName());
		const char* filename_string = ToCString(filename);
		int linenum = message->GetLineNumber();
		fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, exception_string);
		// Print line of source code.
		v8::String::Utf8Value sourceline(message->GetSourceLine());
		const char* sourceline_string = ToCString(sourceline);
		fprintf(stderr, "%s\n", sourceline_string);
		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) {
			fprintf(stderr, " ");
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) {
			fprintf(stderr, "^");
		}
		fprintf(stderr, "\n");
		v8::String::Utf8Value stack_trace(try_catch->StackTrace());
		if (stack_trace.length() > 0) {
			const char* stack_trace_string = ToCString(stack_trace);
			fprintf(stderr, "%s\n", stack_trace_string);
		}
	}
}


CINDER_APP_NATIVE( ProcessApp, RendererGl )
