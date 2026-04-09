const tsPlugin = require("@typescript-eslint/eslint-plugin");
const tsParser = require("@typescript-eslint/parser");
const reactPlugin = require("eslint-plugin-react");
const reactHooksPlugin = require("eslint-plugin-react-hooks");

module.exports = [
  {
    files: ["src/**/*.{ts,tsx}"],
    languageOptions: {
      parser: tsParser,
      parserOptions: {
        ecmaVersion: "latest",
        sourceType: "module",
        ecmaFeatures: { jsx: true },
      },
    },
    plugins: {
      "@typescript-eslint": tsPlugin,
      react: reactPlugin,
      "react-hooks": reactHooksPlugin,
    },
    settings: {
      react: { version: "detect" },
    },
    rules: {
      ...tsPlugin.configs.recommended.rules,
      ...reactPlugin.configs.recommended.rules,
      ...reactHooksPlugin.configs.recommended.rules,
      "react/no-unescaped-entities": "off", // NOSONAR S1534 — intentional override of spread
      "@typescript-eslint/ban-ts-comment": "off", // NOSONAR S1534 — intentional override of spread
      "@typescript-eslint/no-empty-object-type": "off",
      "no-debugger": "error",
      "prefer-const": "error",
      "no-shadow": "off",
      "@typescript-eslint/no-shadow": "off",
      // "@typescript-eslint/ban-types": "warn",
      "@typescript-eslint/no-explicit-any": "off",
      "@typescript-eslint/no-unused-vars": "off",
      "react/prop-types": "off",
      "react/display-name": "off",
    },
  },
  {
    ignores: ["node_modules/**", "**/PawsApi.ts", "www/**", "dist/**"],
  },
];
