public class DukBridge
{
	private long env;

	public DukBridge(String modPath, NativeModuleLoader modLoader) throws Exception {
		long env = jsCreateEnv(modPath);
		if (env == 0L) {
			throw new Exception("Failed to create DukBridge.");
		}
		if (modLoader != null) {
			int ret = jsAddModuleLoader(env, modLoader);
			if (ret != 0) {
				jsDestroyEnv(env);
				throw new Exception("Failed to add module loader");
			}
		}
		this.env = env;
	}

	protected void finalize() {
		jsDestroyEnv(this.env);
	}

	public void setFileReader(FileReader fr) {
		jsSetFileReader(this.env, fr);
	}

	public Object eval(String jsCode) {
		try {
			byte[] b = jsCode.getBytes("utf-8");
			return jsEval(this.env, b);
		} catch (Exception e) {
			return null;
		}
	}

	public Object evalBytes(byte[] jsCode) {
		return jsEval(this.env, jsCode);
	}

	public Object evalFile(String scriptFile) {
		return jsEvalFile(this.env, scriptFile);
	}

	public boolean registerFileFunc(String scriptFile, String funcName) {
		int res = jsRegisterFileFunc(this.env, scriptFile, funcName);
		return (res == 0);
	}

	public boolean registerCodeFunc(String jsCode, String funcName) {
		int res = jsRegisterCodeFunc(this.env, jsCode, funcName);
		return (res == 0);
	}

	public void unregisterFunc(String funcName) {
		jsUnregisterFunc(this.env, funcName);
	}

	public Object callFunc(String funcName, Object ...args) throws Exception {
		NormalizedArgs nargs = J2CHelper.normalizeArgs(args);
		return jsCallFunc(this.env, funcName, nargs.fmt, nargs.args);
	}

	public Object callFileFunc(String scriptFile, Object ...args) throws Exception {
		NormalizedArgs nargs = J2CHelper.normalizeArgs(args);
		return jsCallFileFunc(this.env, scriptFile, nargs.fmt, nargs.args);
	}

	public boolean registerJavaFunc(String funcName, NativeFunc nativeFunc) {
		int ret = jsRegisterJavaFunc(this.env, funcName, nativeFunc);
		return (ret == 0);
	}

	public void unregisterJavaFunc(String funcName) {
		jsUnregisterJavaFunc(this.env, funcName);
	}

	private native long jsCreateEnv(String modPath);
	private native void jsDestroyEnv(long env);
	private native void jsSetFileReader(long env, FileReader fr);
	private native Object jsEval(long env, byte[] jsCode);
	private native Object jsEvalFile(long env, String scriptFile);
	private native int jsRegisterFileFunc(long env, String scriptFile, String funcName);
	private native int jsRegisterCodeFunc(long env, String jsCode, String funcName);
	private native void jsUnregisterFunc(long env, String funcName);
	private native Object jsCallFunc(long env, String funcName, String fmt, Object[] args);
	private native Object jsCallFileFunc(long env, String scriptFile, String fmt, Object[] args);
	private native int jsRegisterJavaFunc(long env, String funcName, NativeFunc nativeFunc);
	private native void jsUnregisterJavaFunc(long env, String funcName);
	private native int jsAddModuleLoader(long env, NativeModuleLoader modLoader);

	static {
		System.loadLibrary("dukjs");
	}
}
