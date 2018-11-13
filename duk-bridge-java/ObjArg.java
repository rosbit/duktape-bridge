public class ObjArg
{
	final public static char af_string  = 's'; // printable string
	final public static char af_buffer  = 'B'; // binary octects
	final public static char af_jarray  = 'a'; // JSON array
	final public static char af_jobject = 'o'; // JSON object

	public char type;
	public byte[] arg;
}
