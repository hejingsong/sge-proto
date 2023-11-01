from distutils.core import setup, Extension


def main():
	src = [
		"../core/dict.c",
		"../core/encoder.c",
		"../core/parser.c",
		"result.c",
		"sgeproto_module.c"
	]

	setup(
		name="sgeProto",
		version="0.0.1",
		description="Python interface for sgeProto",
		author="hejingsong",
		author_email="240197153@qq.com",
		ext_modules=[
			Extension(name="sgeProto", sources=src)
		]
	)


if __name__ == "__main__":
	main()
