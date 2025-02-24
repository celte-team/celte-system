# Development Norms

This document describes the process to follow when developing a new feature in the Celte project.

## TL;DR

The purpose of a feature is not just to provide “good code,” but to implement a working solution that can be tested and validated by the team. Code should be well-documented and tested. Additionally, all code must be reviewed by the team before being merged into the main branch.

Do not hesitate to ask for help if you are stuck or have any questions.

## General Rules

    • A commit message should be clear and concise, explaining the changes made in the commit following the Git Commit Message Guidelines :
        - fix:, add:, del:, update:, etc. with ':'
	• No header files: Header files should not be used in this project.
	• No irrelevant comments: All unnecessary or redundant comments must be removed to keep the code clean.
	• Consistent formatting: Follow the coding style of each language (see below).
	• No hardcoding: Avoid hardcoding values in the code. Use constants or configuration files instead.
	• Test your code with the rest of the new feature before pushing it.
	• Make sure any old thing is broke!

## Coding Style

`.cpp`

C++ is the main language used in the project, and the coding style follows the Google C++ Style Guide.
	•    Naming conventions: CamelCase is used for variables, functions, and classes.
	•    Documentation: The code should be well-documented using comments and Doxygen-style documentation.
	•    Testing: All code should be tested using the Google Test framework.

	Naming conventions:
		classes & struct: camel case with Capital letter at the beginning, e.g: ContainerRegistry
		public methods: camel case with Capital letter at the beginning, e.g: int GetProperty()
		private methods: camel case with leading double underscore, e.g: void __doPrivateWork()
		private members: camel case with leading underscore, e.g: int _nEntities
		public members: camel case, e.g: int publicProp

		avoid using public members as much as possible

		doxygen comments using the /// syntax instead of the /* * */
		document class and struct members with ///<

		e.g:

		/// @brief my method does this
		/// @param int x does that
		/// @return void
		void Method(int x);

		int member; //< This member is used to ...

`.cs`

C# is used for the GUI part of the project, and the coding style follows the Microsoft C# Coding Conventions.
	•    Naming conventions: CamelCase is used for variables, functions, and classes.
	•    Documentation: The code should be well-documented using comments and XML documentation.
	•    Testing: No testing is required for the C# part of the project at the moment.

`.gd`

GDScript is used for the game logic, and the coding style follows the Godot Engine Coding Style.
	•    Naming conventions: snake_case is used for variables, functions, and classes.
	•    Documentation: The code should be well-documented using comments.
	•    Testing: No testing is required for the GDScript part of the project at the moment.
