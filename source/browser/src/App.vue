<template>
  <v-app>
    <v-app-bar
      app
      color="#17a2b7"
      dark
    >
      <div class="d-flex align-center">
        <v-img
          alt="TinyIoT Logo"
          class="shrink mr-2"
          contain
          src="./assets/TinyIoT Logo.png"
          transition="scale-transition"
          width="70"
        />

        <v-img
          alt="TinyIoT Name"
          class="shrink mt-1 hidden-sm-and-down"
          contain
          min-width="100"
          src="./assets/TinyIoT Name.png"
          width="110"
        />

        <span> Resource Browser </span>
      </div>

      <v-spacer></v-spacer>

      <!-- <v-btn
        href="https://github.com/vuetifyjs/vuetify/releases/latest"
        target="_blank"
        text
      >
        <span class="mr-2">Latest Release</span>
        <v-icon>mdi-open-in-new</v-icon>
      </v-btn> -->
    </v-app-bar>

    <v-main>
      <Home/>
    </v-main>
  </v-app>
</template>

<script>
import Home from './components/Home';

export default {
  name: 'App',

  components: {
    Home,
  },

  data: () => ({
    //
  }),
};
</script>

<style>
span {
  opacity: 0.8;
}
</style>