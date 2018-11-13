public interface NativeModuleLoader
{
	Class<?> LoadModule(String modName);
	void FinalizeModule(String modName, Class<?> cls);
}
