/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    './pages/**/*.{js,ts,jsx,tsx,mdx}',
    './components/**/*.{js,ts,jsx,tsx,mdx}',
    './app/**/*.{js,ts,jsx,tsx,mdx}',
  ],
  theme: {
    extend: {
      colors: {
        'playstation-blue': '#003087',
        'playstation-light-blue': '#0070f3',
        'playstation-dark': '#1a1a1a',
      },
      fontFamily: {
        'playstation': ['Arial', 'sans-serif'],
      },
    },
  },
  plugins: [],
}
